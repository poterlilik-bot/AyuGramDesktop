// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Window {
class SessionController;
} // namespace Window

class ChannelData;

void ShowAdminMembersBox(
    not_null<Window::SessionController*> controller,
    not_null<ChannelData*> channel);
