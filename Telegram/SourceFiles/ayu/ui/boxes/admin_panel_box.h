// This is the source code of AyuGram for Desktop.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Window {
class SessionController;
} // namespace Window

class PeerData;

// Adaptive moderation panel for groups/channels.
void ShowAdminPanel(
    not_null<Window::SessionController*> controller,
    not_null<PeerData*> peer);
