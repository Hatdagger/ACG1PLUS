# Writes OUT: the file IN as a C byte array NAME (+ NAME_size). Used for game.toml,
# so the SETTINGS pages know the game's own defaults before the game is built.
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" len)
math(EXPR size "${len} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
file(WRITE "${OUT}" "/* Made by launcher2/cmake/embed_text.cmake from ${NAME_OF_FILE}. */\n"
                    "const unsigned char ${NAME}[] = {\n${bytes}0x00\n};\n"
                    "const unsigned int ${NAME}_size = ${size};\n")
