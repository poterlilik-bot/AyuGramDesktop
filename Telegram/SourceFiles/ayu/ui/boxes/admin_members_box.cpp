// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/admin_members_box.h"

#include "apiwrap.h"
#include "api/api_chat_participants.h"
#include "boxes/peer_list_box.h"
#include "data/data_channel.h"
#include "data/data_chat_participant_status.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"

namespace {

using Flag = ChatRestriction;

class MembersController final : public PeerListController {
public:
    explicit MembersController(not_null<ChannelData*> channel)
    : _channel(channel) {
    }
    Main::Session &session() const override {
        return _channel->session();
    }
    void prepare() override {
        delegate()->peerListSetTitle(rpl::single(u"Select members"_q));
        const auto channel = _channel;
        channel->session().api().chatParticipants().requestForAdd(channel,
            [=](const Api::ChatParticipants::TLMembers &data) {
                const auto parsed = Api::ChatParticipants::Parse(channel, data);
                for (const auto &p : parsed.list) {
                    if (!p.isUser()) {
                        continue;
                    }
                    const auto user = channel->owner().user(p.userId());
                    if (user->isSelf()
                        || delegate()->peerListFindRow(user->id.value)) {
                        continue;
                    }
                    delegate()->peerListAppendRow(
                        std::make_unique<PeerListRow>(user));
                }
                delegate()->peerListRefreshRows();
            });
    }
    void rowClicked(not_null<PeerListRow*> row) override {
        delegate()->peerListSetRowChecked(row, !row->checked());
    }
private:
    const not_null<ChannelData*> _channel;
};

[[nodiscard]] ChatRestrictions LockRights() {
    return Flag::SendStickers | Flag::SendGifs | Flag::SendGames
        | Flag::SendInline | Flag::SendPolls | Flag::SendPhotos
        | Flag::SendVideos | Flag::SendVideoMessages | Flag::SendMusic
        | Flag::SendVoiceMessages | Flag::SendFiles | Flag::SendOther
        | Flag::EmbedLinks;
}

} // namespace

void ShowAdminMembersBox(
        not_null<Window::SessionController*> controller,
        not_null<ChannelData*> channel) {
    auto listController = std::make_unique<MembersController>(channel);
    controller->show(Box<PeerListBox>(std::move(listController), [=](
            not_null<PeerListBox*> box) {
        box->addButton(rpl::single(u"Ban selected"_q), [=] {
            const auto peers = box->collectSelectedRows();
            for (const auto &peer : peers) {
                channel->session().api().chatParticipants().kick(
                    channel, peer, ChatRestrictionsInfo());
            }
            Ui::Toast::Show(
                QString("Banned %1 member(s).").arg(int(peers.size())));
            box->closeBox();
        });
        box->addButton(rpl::single(u"Mute selected"_q), [=] {
            const auto peers = box->collectSelectedRows();
            const auto rights = ChatRestrictionsInfo(LockRights(), 0);
            for (const auto &peer : peers) {
                Api::ChatParticipants::Restrict(
                    channel, peer, ChatRestrictionsInfo(), rights,
                    [] {}, [](const QString &) {});
            }
            Ui::Toast::Show(
                QString("Muted %1 member(s).").arg(int(peers.size())));
            box->closeBox();
        });
        box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
    }));
}
