#pragma once

#include <stdint.h>
#include <WString.h>

#include "../Playlist.h"

class WebstreamPlaylist : public Playlist {
protected:
	char *url;

public:
	WebstreamPlaylist(const char *_url) : url(nullptr) {
		// url = this->stringCopy(_url);
	}
	WebstreamPlaylist() : url(nullptr) { }
	virtual ~WebstreamPlaylist() override {
	};

	void setUrl(const char *_url) {
		// url = this->stringCopy(_url);
	}

	virtual size_t size() const override { return (url) ? 1 : 0; }
	virtual bool isValid() const override { return (url); }
	virtual const String getAbsolutePath(size_t idx = 0) const override { return url; };
	virtual const String getFilename(size_t idx = 0) const override { return url; };

};
