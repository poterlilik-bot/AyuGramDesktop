// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/delete_chats_box.h"

#include "apiwrap.h"
#include "boxes/filters/edit_filter_chats_list.h"
#include "boxes/peer_list_box.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"

namespace {

using Flag = Data::ChatFilter::Flag;
using Flags = Data::ChatFilter::Flags;

// Resolves the explicitly checked peers plus everything matching the chosen
// type-filters (Contacts / Groups / Channels / ...) into a set of histories.
[[nodiscard]] base::flat_set<not_null<History*>> CollectChats(
		not_null<Main::Session*> session,
		Flags flags,
		const std::vector<not_null<PeerData*>> &selected) {
	auto result = base::flat_set<not_null<History*>>();
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

void ConfirmAndDelete(
		not_null<Window::SessionController*> controller,
		base::flat_set<not_null<History*>> chats) {
	if (chats.empty()) {
		return;
	}
	const auto count = int(chats.size());
	const auto deleteSure = [=](Fn<void()> close) {
		for (const auto &history : chats) {
			const auto peer = history->peer;
			peer->session().api().deleteConversation(peer, false);
		}
		close();
	};
	controller->show(Ui::MakeConfirmBox({
		.text = u"Delete %1 chat(s)? This cannot be undone."_q.arg(count),
		.confirmed = deleteSure,
		.confirmText = tr::lng_box_delete(),
		.confirmStyle = &st::attentionBoxButton,
	}));
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
		rpl::single(u"Delete chats"_q),
		options,
		Flags(0),
		base::flat_set<not_null<History*>>(),
		1'000'000,
		[] {});
	const auto raw = listController.get();
	auto initBox = [=](not_null<PeerListBox*> box) {
		box->addButton(tr::lng_box_delete(), [=] {
			ConfirmAndDelete(
				controller,
				CollectChats(
					session,
					raw->chosenOptions(),
					box->collectSelectedRows()));
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	controller->show(
		Box<PeerListBox>(std::move(listController), std::move(initBox)));
}
