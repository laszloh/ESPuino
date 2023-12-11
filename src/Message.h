#pragma once

#include <memory>

class Msg {
public:
	// all the message typoes we support (currently only audio messages)
	enum MsgType {
		AudioPlayerMsg
	};

	Msg(MsgType type)
		: type_(type) { }
	virtual ~Msg() = default;

	virtual std::unique_ptr<Msg> move() {
		return std::unique_ptr<Msg>(new Msg(std::move(*this)));
	}

	// disable copying
	Msg(const Msg &) = delete;
	Msg &operator=(const Msg &) = delete;

	MsgType type() const { return type_; }

protected:
	// Enbale move operators
	Msg(Msg &&) = default;
	Msg &operator=(Msg &&) = default;

private:
	MsgType type_;
};

class AudioMsg : public Msg {
public:
	// These Events control the AudioPlayer task
	enum Event {
		TrackCommand,
		VolumeCommand,
		PlaylistCommand
	};

	AudioMsg(Event event, uint32_t data = 0)
		: Msg(MsgType::AudioPlayerMsg)
		, event_(event)
		, data_ {data} { }
	virtual ~AudioMsg() = default;

	virtual std::unique_ptr<Msg> move() {
		return std::unique_ptr<Msg>(new AudioMsg(std::move(*this)));
	}

	// disable copying
	AudioMsg(const AudioMsg &) = delete;
	AudioMsg &operator=(const AudioMsg &) = delete;

	Event event() const { return event_; }
	uint32_t data() const { return data_; }

protected:
	// Enbale move operators
	AudioMsg(AudioMsg &&) = default;
	AudioMsg &operator=(AudioMsg &&) = default;

private:
	Event event_;
	uint32_t data_;
};

template <typename PayloadType>
class AudioDataMsg : public AudioMsg {
public:
	template <typename... Args>
	AudioDataMsg(Event event, Args &&...args)
		: AudioMsg(event)
		, payload_(PayloadType(std::forward<Args>(args)...)) { }
	virtual ~AudioDataMsg() = default;

	virtual std::unique_ptr<Msg> move() {
		return std::unique_ptr<Msg>(new AudioDataMsg<PayloadType>(std::move(*this)));
	}

	// disable copying
	AudioDataMsg(const AudioDataMsg &) = delete;
	AudioDataMsg &operator=(const AudioDataMsg &) = delete;

	const PayloadType &playload() const { return payload_; }
    PayloadType &playload() { return payload_; }

protected:
	// Enbale move operators
	AudioDataMsg(AudioDataMsg &&) = default;
	AudioDataMsg &operator=(AudioDataMsg &&) = default;

private:
	PayloadType payload_;
};

void Message_Send(Msg &&msg);
