#pragma once

#include <stdlib.h>
#include <vector>
#include <string>

using Playlist = std::vector<std::string>;

// Release previously allocated memory
inline void freePlaylist(Playlist *playlist) {
	delete playlist;
	playlist = nullptr;
}
