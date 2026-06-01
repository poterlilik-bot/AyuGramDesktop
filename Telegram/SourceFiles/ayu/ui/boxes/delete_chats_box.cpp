// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/delete_chats_box.h"

#include "apiwrap.h"
#include "base/unique_qptr.h"
#include "boxes/filters/edit_filter_chats_list.h"
#include "boxes/peer_list_box.h"
#include "data/data_channel.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/notify/data_notify_settings.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtGui/QCursor>

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

void Done(const QString &what, int count) {
    Ui::Toast::Show(what + QString(" %1 chat(s).").arg(count));
}

void ApplyArchive(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().api().toggleHistoryArchived(history, true, [] {});
    }
    Done("Archived", int(chats.size()));
}

void ApplyRead(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().data().histories().readInbox(history);
    }
    Done("Marked read", int(chats.size()));
}

void ApplyMute(const Chats &chats, bool mute) {
    for (const auto &history : chats) {
        if (mute) {
            history->owner().notifySettings().update(history, { .forever = true });
        } else {
            history->owner().notifySettings().update(history, { .unmute = true });
        }
    }
    Done(mute ? "Muted" : "Unmuted", int(chats.size()));
}

void ApplyPin(const Chats &chats, bool pin) {
    for (const auto &history : chats) {
        history->owner().setChatPinned(history, FilterId(), pin);
    }
    Done(pin ? "Pinned" : "Unpinned", int(chats.size()));
}

void ApplyClear(const Chats &chats) {
    for (const auto &history : chats) {
        history->session().api().clearHistory(history->peer, false);
    }
    Done("Cleared", int(chats.size()));
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
    Done("Left", int(chats.size()));
}

void ApplyDelete(const Chats &chats) {
    for (const auto &history : chats) {
        history->peer->session().api().deleteConversation(history->peer, false);
    }
    Done("Deleted", int(chats.size()));
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
        struct State {
            base::unique_qptr<Ui::PopupMenu> menu;
        };
        const auto state = box->lifetime().make_state<State>();
        const auto gather = [=] {
            return CollectChats(
                session, raw->chosenOptions(), box->collectSelectedRows());
        };
        box->addButton(rpl::single(u"Actions"_q), [=] {
            const auto chats = gather();
            if (chats.empty()) {
                Ui::Toast::Show(u"Select chats first."_q);
                return;
            }
            state->menu.emplace(box, st::defaultPopupMenu);
            const auto confirmThen = [=](
                    QString text,
                    QString button,
                    Fn<void()> act) {
                controller->show(Ui::MakeConfirmBox({
                    .text = text.arg(int(chats.size())),
                    .confirmed = [=](Fn<void()> close) { act(); close(); },
                    .confirmText = button,
                    .confirmStyle = &st::attentionBoxButton,
                }));
            };
            state->menu->addAction(u"Archive"_q, [=] { ApplyArchive(chats); });
            state->menu->addAction(u"Mark as read"_q, [=] { ApplyRead(chats); });
            state->menu->addAction(u"Mute"_q, [=] { ApplyMute(chats, true); });
            state->menu->addAction(u"Unmute"_q, [=] { ApplyMute(chats, false); });
            state->menu->addAction(u"Pin"_q, [=] { ApplyPin(chats, true); });
            state->menu->addAction(u"Unpin"_q, [=] { ApplyPin(chats, false); });
            state->menu->addAction(u"Clear history"_q, [=] {
                confirmThen(u"Clear history in %1 chat(s)?"_q, u"Clear"_q,
                    [=] { ApplyClear(chats); });
            });
            state->menu->addAction(u"Leave"_q, [=] {
                confirmThen(u"Leave %1 chat(s)?"_q, u"Leave"_q,
                    [=] { ApplyLeave(chats); });
            });
            state->menu->addAction(u"Delete"_q, [=] {
                confirmThen(u"Delete %1 chat(s)? Cannot be undone."_q,
                    u"Delete"_q, [=] { ApplyDelete(chats); });
            });
            state->menu->popup(QCursor::pos());
        });
        box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
    };
    controller->show(
        Box<PeerListBox>(std::move(listController), std::move(initBox)));
}
