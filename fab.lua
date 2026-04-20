local c = require("lang_c")
local ar = require("ar")

-- Options
local options = {
    build_type = fab.option("buildtype", { "release", "debug", "fuzz" }) or "release" --[[@as string]],
    build_tools = fab.option("build_tools", "boolean") or true --[[@as boolean]]
}

-- Tools
local cc = c.get_clang()
if cc == nil then
    error("No viable C compiler found")
end

local ar = ar.get_ar()
if ar == nil then
    error("No viable AR found")
end

-- Flags
local c_flags = {
    "-std=gnu23",

    -- Warnings
    "-Werror",
    "-Wall",
    "-Wextra",
    "-Wvla",
    "-Wimplicit-fallthrough",
    "-Wmissing-field-initializers",

    -- Misc
    "-fdiagnostics-color=always",

    -- Build Type Flags
    table.unpack(({
        release = {
            "-O3",
            "-DNDEBUG"
        },
        debug = {
            "-O0",
            "-g",
            "-fstack-usage"
        },
        fuzz = {
            "-O1",
            "-g",
            "-fsanitize=fuzzer,undefined",
            "-DFIL_FUZZ"
        }
    })[options.build_type])
}

-- Build
local c_sources = sources(fab.glob("src/**/*.c"))
local include_dirs = { c.include_dir("include") }

local objects = cc:generate(c_sources, c_flags, include_dirs)

local install = {}
if options.build_type == "fuzz" then
    install["bin/filfuzz"] = cc:link("filfuzz", objects, c_flags)
else
    local static_library = ar:create("libfil.a", objects)

    install["lib/libfil.a"] = static_library

    if options.build_tools then
        local gunzip_object = cc:compile_object("filgunzip.o", fab.def_source("tools/filgunzip.c"), include_dirs, c_flags)
        local gunzip_binary = cc:link("filgunzip", { gunzip_object, static_library }, c_flags)

        install["bin/filgunzip"] = gunzip_binary
    end
end

return {
    install = install
}
