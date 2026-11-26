# Writes OUT: the two font files REGULAR and BOLD as C byte arrays
# (acg_font_regular / acg_font_bold + their sizes), so the launcher is one file.
function(embed name path out_var)
    file(READ "${path}" hex HEX)
    string(LENGTH "${hex}" len)
    math(EXPR size "${len} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
    set(${out_var} "const unsigned char ${name}[] = {\n${bytes}\n};\nconst unsigned int ${name}_size = ${size};\n" PARENT_SCOPE)
endfunction()

embed(acg_font_regular "${REGULAR}" a)
embed(acg_font_bold "${BOLD}" b)
file(WRITE "${OUT}" "/* Made by launcher2/cmake/embed_fonts.cmake from recomp-ui's LatoLatin fonts\n"
                    " * (Copyright (c) 2010-2015 Lukasz Dziedzic, SIL Open Font License 1.1). */\n"
                    "${a}\n${b}")
