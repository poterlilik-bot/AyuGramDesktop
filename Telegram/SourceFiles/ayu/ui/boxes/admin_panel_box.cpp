// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/admin_panel_box.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"
#include "styles/style_settings.h"

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
        box->addButton(tr::lng_close(), [=] { box->closeBox(); });
    }));
}
