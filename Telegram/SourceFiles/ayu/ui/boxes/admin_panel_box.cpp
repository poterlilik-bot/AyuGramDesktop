// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/admin_panel_box.h"

#include "apiwrap.h"
#include "ayu/ui/boxes/admin_members_box.h"
#include "data/data_channel.h"
#include "data/data_chat_participant_status.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "api/api_chat_participants.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"
#include "styles/style_settings.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace {

void SetSlowmode(not_null<ChannelData*> channel, int seconds) {
    const auto api = &channel->session().api();
    api->request(MTPchannels_ToggleSlowMode(
        channel->inputChannel(),
        MTP_int(seconds)
    )).done([=](const MTPUpdates &result) {
        api->applyUpdates(result);
        channel->setSlowmodeSeconds(seconds);
        Ui::Toast::Show(u"Slow mode updated."_q);
    }).send();
}

void SetDefaultRestrictions(not_null<PeerData*> peer, ChatRestrictions rights) {
    const auto api = &peer->session().api();
    api->request(MTPmessages_EditChatDefaultBannedRights(
        peer->input(),
        RestrictionsToMTP({ rights, 0 })
    )).done([=](const MTPUpdates &result) {
        api->applyUpdates(result);
        Ui::Toast::Show(u"Permissions updated."_q);
    }).send();
}

void KickDeleted(not_null<ChannelData*> channel) {
    const auto session = &channel->session();
    session->api().chatParticipants().requestForAdd(channel,
        [=](const Api::ChatParticipants::TLMembers &data) {
            const auto parsed = Api::ChatParticipants::Parse(channel, data);
            auto count = 0;
            for (const auto &p : parsed.list) {
                if (!p.isUser()) {
                    continue;
                }
                const auto user = session->data().user(p.userId());
                if (user->flags() & UserDataFlag::Deleted) {
                    session->api().chatParticipants().kick(
                        channel, user, p.restrictions());
                    ++count;
                }
            }
            Ui::Toast::Show(
                QString("Kicked %1 deleted account(s).").arg(count));
        });
}

void KickBots(not_null<ChannelData*> channel) {
    const auto session = &channel->session();
    session->api().chatParticipants().requestForAdd(channel,
        [=](const Api::ChatParticipants::TLMembers &data) {
            const auto parsed = Api::ChatParticipants::Parse(channel, data);
            auto count = 0;
            for (const auto &p : parsed.list) {
                if (!p.isUser()) {
                    continue;
                }
                const auto user = session->data().user(p.userId());
                if (user->isBot()) {
                    session->api().chatParticipants().kick(
                        channel, user, p.restrictions());
                    ++count;
                }
            }
            Ui::Toast::Show(QString("Kicked %1 bot(s).").arg(count));
        });
}
void ExportMembers(not_null<ChannelData*> channel) {
    const auto session = &channel->session();
    session->api().chatParticipants().requestForAdd(channel,
        [=](const Api::ChatParticipants::TLMembers &data) {
            const auto parsed = Api::ChatParticipants::Parse(channel, data);
            auto lines = QStringList();
            for (const auto &p : parsed.list) {
                if (!p.isUser()) {
                    continue;
                }
                const auto user = session->data().user(p.userId());
                lines.append(user->name()
                    + " ["
                    + QString::number(user->id.value)
                    + "]");
            }
            QGuiApplication::clipboard()->setText(lines.join(QChar(10)));
            Ui::Toast::Show(
                QString("Copied %1 member(s).").arg(int(lines.size())));
        });
}

} // namespace

void ShowAdminPanel(
        not_null<Window::SessionController*> controller,
        not_null<PeerData*> peer) {
    const auto channel = peer->asChannel();
    if (!channel) {
        Ui::Toast::Show(u"Admin Panel is available in groups and channels."_q);
        return;
    }
    controller->show(Box([=](not_null<Ui::GenericBox*> box) {
        box->setTitle(rpl::single(u"Admin Panel"_q));
        const auto container = box->verticalLayout();
        const auto add = [&](const QString &text, Fn<void()> callback) {
            const auto button = container->add(object_ptr<Ui::SettingsButton>(
                container,
                rpl::single(text),
                st::settingsButtonNoIcon));
            button->setClickedCallback(std::move(callback));
        };
        add(u"Slow mode: off"_q, [=] { SetSlowmode(channel, 0); });
        add(u"Slow mode: 10s"_q, [=] { SetSlowmode(channel, 10); });
        add(u"Slow mode: 30s"_q, [=] { SetSlowmode(channel, 30); });
        add(u"Slow mode: 1m"_q, [=] { SetSlowmode(channel, 60); });
        add(u"Slow mode: 5m"_q, [=] { SetSlowmode(channel, 300); });
        const auto kLock = ChatRestriction::SendStickers
            | ChatRestriction::SendGifs
            | ChatRestriction::SendGames
            | ChatRestriction::SendInline
            | ChatRestriction::SendPolls
            | ChatRestriction::SendPhotos
            | ChatRestriction::SendVideos
            | ChatRestriction::SendVideoMessages
            | ChatRestriction::SendMusic
            | ChatRestriction::SendVoiceMessages
            | ChatRestriction::SendFiles
            | ChatRestriction::SendOther
            | ChatRestriction::EmbedLinks;
        add(u"Raid Mode: LOCK (mute non-admins)"_q, [=] { SetDefaultRestrictions(peer, kLock); });
        add(u"Raid Mode: UNLOCK"_q, [=] { SetDefaultRestrictions(peer, ChatRestrictions()); });
        add(u"Kick deleted accounts"_q, [=] { KickDeleted(channel); });
        add(u"Kick all bots"_q, [=] { KickBots(channel); });
        add(u"Export members to clipboard"_q, [=] { ExportMembers(channel); });
        add(u"Ban / mute members"_q, [=] { ShowAdminMembersBox(controller, channel); });
        box->addButton(tr::lng_close(), [=] { box->closeBox(); });
    }));
}
