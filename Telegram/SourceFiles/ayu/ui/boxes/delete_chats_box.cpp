// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/delete_chats_box.h"

#include "apiwrap.h"
#include "boxes/filters/edit_filter_chats_list.h"
#include "boxes/peer_list_box.h"
#include "data/data_channel.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"

namespace {

using Flag = Data::ChatFilter::Flag;
using Flags = Data::ChatFilter::Flags;
using Chats = base::flat_set<not_null<History*>>;

[[nodiscard]] Chats CollectChats(
        not_null<Main::Session*> session,
        Flags flags,
        const std::vector<not_null<PeerData*>> &selected) {
    auto result = Chats();
    for (const auto &peer : selected) {
        result.emplace(session->data().history(peer));
    }
    if (flags != Flags(0)) {
        const auto filter = Data::ChatFilter(
            0, {}, QString(), std::nullopt, flags, {}, {}, {});
        const auto append = [&](auto chats) {
            for (const auto &row : chats->all()) {
                if (const auto history = row->history()) {
                    if (filter.contains(history)) {
                        result.emplace(history);
                    }
                }
            }
        };
        append(session->data().chatsList()->indexed());
        if (const auto f = session->data().folderLoaded(Data::Folder::kId)) {
            append(f->chatsList()->indexed());
        }
    }
    return result;
}

void ApplyArchive(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().api().toggleHistoryArchived(history, true, [] {});
    }
    Ui::Toast::Show(QString("Archived %1 chat(s).").arg(int(chats.size())));
}

void ApplyRead(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().data().histories().readInbox(history);
    }
    Ui::Toast::Show(QString("Marked %1 chat(s) as read.").arg(int(chats.size())));
}

void ApplyClear(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().api().clearHistory(history->peer, false);
    }
    Ui::Toast::Show(QString("Cleared %1 chat(s).").arg(int(chats.size())));
}

void ApplyLeave(const Chats &chats) {
    for (const auto &history : chats) {
        const auto peer = history->peer;
        if (const auto channel = peer->asChannel()) {
            peer->session().api().leaveChannel(channel);
        } else {
            peer->session().api().deleteConversation(peer, false);
        }
    }
    Ui::Toast::Show(QString("Left %1 chat(s).").arg(int(chats.size())));
}

void ApplyDelete(const Chats &chats) {
    for (const auto &history : chats) {
        history->peer->session().api().deleteConversation(history->peer, false);
    }
    Ui::Toast::Show(QString("Deleted %1 chat(s).").arg(int(chats.size())));
}

} // namespace

void ShowDeleteChatsBox(not_null<Window::SessionController*> controller) {
    const auto session = &controller->session();
    const auto options = Flag::Contacts
        | Flag::NonContacts
        | Flag::Groups
        | Flag::Channels
        | Flag::Bots;
    auto listController = std::make_unique<EditFilterChatsListController>(
        session,
        rpl::single(u"Bulk chat actions"_q),
        options,
        Flags(0),
        base::flat_set<not_null<History*>>(),
        1000000,
        [] {});
    const auto raw = listController.get();
    auto initBox = [=](not_null<PeerListBox*> box) {
        const auto gather = [=] {
            return CollectChats(
                session, raw->chosenOptions(), box->collectSelectedRows());
        };
        const auto confirm = [=](
                QString text,
                QString button,
                Fn<void(Chats)> apply) {
            auto chats = gather();
            if (chats.empty()) {
                return;
            }
            controller->show(Ui::MakeConfirmBox({
                .text = text.arg(int(chats.size())),
                .confirmed = [=](Fn<void()> close) { apply(chats); close(); },
                .confirmText = button,
                .confirmStyle = &st::attentionBoxButton,
            }));
        };
        box->addButton(rpl::single(u"Archive"_q), [=] {
            auto chats = gather();
            if (!chats.empty()) { ApplyArchive(chats); }
        });
        box->addButton(rpl::single(u"Read"_q), [=] {
            auto chats = gather();
            if (!chats.empty()) { ApplyRead(chats); }
        });
        box->addButton(rpl::single(u"Clear"_q), [=] {
            confirm(u"Clear history in %1 chat(s)?"_q, u"Clear"_q,
                [](Chats c) { ApplyClear(c); });
        });
        box->addButton(rpl::single(u"Leave"_q), [=] {
            confirm(u"Leave %1 chat(s)?"_q, u"Leave"_q,
                [](Chats c) { ApplyLeave(c); });
        });
        box->addButton(rpl::single(u"Delete"_q), [=] {
            confirm(u"Delete %1 chat(s)? Cannot be undone."_q, u"Delete"_q,
                [](Chats c) { ApplyDelete(c); });
        });
        box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });
    };
    controller->show(
        Box<PeerListBox>(std::move(listController), std::move(initBox)));
}
