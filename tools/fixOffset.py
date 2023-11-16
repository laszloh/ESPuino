
Import("env")  # pylint: disable=undefined-variable

board = env.BoardConfig()

env.Replace(
    ESP32_APP_OFFSET=board.get("upload.offset_address", "0x10000"),
)
