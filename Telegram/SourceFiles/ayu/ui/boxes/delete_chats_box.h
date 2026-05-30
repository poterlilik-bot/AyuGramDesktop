// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Window {
class SessionController;
} // namespace Window

// Opens a box to multi-select chats (by checkbox and by type-filters) and
// bulk-delete the selected conversations after a confirmation.
void ShowDeleteChatsBox(not_null<Window::SessionController*> controller);
