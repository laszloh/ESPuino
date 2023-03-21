#pragma once

#include <stdint.h>
#include <WString.h>

#include "../Playlist.h"

template <typename TAllocator>
class WebstreamPlaylistAlloc : public PlaylistAlloc<TAllocator> {
protected:
	char *url;

public:
	explicit WebstreamPlaylistAlloc(const char *_url, TAllocator alloc = TAllocator()) : PlaylistAlloc<TAllocator>(alloc), url(nullptr) {
		url = this->stringCopy(_url);
	}
	WebstreamPlaylistAlloc(TAllocator alloc = TAllocator()) : PlaylistAlloc<TAllocator>(alloc), url(nullptr) { }
	virtual ~WebstreamPlaylistAlloc() override {
		this->deallocate(url);
	};

	void setUrl(const char *_url) {
		if(_url) {
			this->deallocate(url);
		}
		url = this->stringCopy(_url);
	}

	virtual size_t size() const override { return (url) ? 1 : 0; }
	virtual bool isValid() const override { return (url); }
	virtual const String getAbsolutePath(size_t idx = 0) const override { return url; };
	virtual const String getFilename(size_t idx = 0) const override { return url; };

};
using WebstreamPlaylist = WebstreamPlaylistAlloc<DefaultPsramAllocator>;
