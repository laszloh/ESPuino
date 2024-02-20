#pragma once

#include <Stream.h>
#include <WString.h>
#include <vector>

namespace media {

/// @brief Class for providing
class Artwork : public Stream {
public:
	Artwork() = default;
	virtual ~Artwork() = default;

	// copy operators
	Artwork(const Artwork &) = default;
	Artwork &operator=(const Artwork &) = default;

	// move operators
	Artwork(Artwork &&) = default;
	Artwork &operator=(Artwork &&) = default;

	String mimeType {String()};
};

class RamArtwork : public Artwork {
public:
	RamArtwork() = default;

	void setContent(const uint8_t *data, size_t len) {
		this->data.resize(len);
		std::copy(data, data + len, this->data.begin());
		readIt = this->data.cbegin();
	}

	void setContent(const std::vector<uint8_t> &data) {
		this->data = data;
	}

	void setContent(std::vector<uint8_t> &&data) {
		this->data = std::move(data);
	}

	// overrides for Stream

	/// @see https://www.arduino.cc/reference/en/language/functions/communication/stream/streamavailable/
	virtual bool available() override {
		return readIt != data.end();
	}

	/// @see https://www.arduino.cc/reference/en/language/functions/communication/stream/streamread/
	virtual int read() override {
		int c = *readIt;
		readIt++;
		return c;
	}

	///@see https://www.arduino.cc/reference/en/language/functions/communication/stream/streampeek/
	virtual int peek() override {
		return *readIt;
	}

protected:
	std::vector<uint8_t> data {std::vector<uint8_t>()};
	std::vector<uint8_t>::const_iterator readIt {};
};

struct Metadata {
	Metadata() = default;
	virtual Metadata() = default;

	// copy operators for base class are deleted because std::unique_ptr
	Metadata(const Metadata &) = delete;
	Metadata &operator=(const Metadata &) = delete;

	// move operators
	Metadata(Metadata &&) = default;
	Metadata &operator=(Metadata &&) = default;

	String title {String()}; //< title of the track
	String artist {String()}; //< artist
	size_t duration;
	String albumTitle {String()}; //< album title
	String albumArtist {String()}; //< album artist, use artist if not set
	String displayTitle {String()}; //< overrides title if set
	size_t trackNumber; //< track number _in the album_
	size_t totalTrackCount; //< total number of track _in the album_
	size_t chapter; //< the chapter number if this is a audiobook
	uint8_t releaseYear; //< year of the song/album release
	uint8_t releaseMonth; //< month of the song/album release
	uint8_t releaseDay; //< day of the song/album release

	// embedded artwork
	std::unique_ptr<Artwork> artwork;

	/// @brief Compare operator overload for MediaMetadata, returns true if lhs==rhs
	friend bool operator==(Metadata const &lhs, Metadata const &rhs) {
		return lhs.title == rhs.title && lhs.artist == rhs.artist && lhs.albumTitle == rhs.albumTitle
			&& lhs.albumArtist == rhs.albumArtist && lhs.displayTitle == rhs.displayTitle
			&& lhs.trackNumber == rhs.trackNumber && lhs.totalTrackCount == rhs.totalTrackCount
			&& lhs.releaseYear == rhs.releaseYear && lhs.releaseMonth == rhs.releaseMonth
			&& lhs.releaseDay == rhs.releaseDay && lhs.artworkMimeType == rhs.artworkMimeType
			&& lhs.artworkData == rhs.artworkData && lhs.artworkUri == rhs.artworkUri;
	}
};

} // namespace media

/// @brief Object representing a single entry in the playlist
struct MediaItem {
	const String uri {String()}; //< playable uri of the entry
	std::unique_ptr<media::Metadata> metadata {nullptr}; //< optional metadata for the entry

	/// @brief Compare operator overload for MediaItem, returns true if lhs==rhs
	friend bool operator==(MediaItem const &lhs, MediaItem const &rhs) {
		const auto comparePointer = [](const std::unique_ptr<Metadata> &lhs, const std::unique_ptr<Metadata> &rhs) -> bool {
			if (lhs == rhs) {
				return true;
			}
			if (lhs && rhs) {
				return *lhs == *rhs;
			}
			return false;
		};

		return lhs.uri == rhs.uri && comparePointer(lhs.metadata, rhs.metadata);
	}

	MediaItem() = default;
	MediaItem(const String &uri)
		: uri(uri) { }
	virtual ~MediaItem() = default;

	//  copy operators for base class are deleted because std::unique_ptr
	MediaItem(const MediaItem &) = delete;
	MediaItem &operator=(const MediaItem &) = delete;

	// move operators
	MediaItem(MediaItem &&) = default;
	MediaItem &operator=(MediaItem &&) = default;
};
