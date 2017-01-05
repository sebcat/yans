#!/usr/bin/env lua
-- vim: set tabstop=2 shiftwidth=2 cc=80 expandtab:

-- Builds an amalgamation of the lua interpreter. Can maybe be used for
-- other C projects with a similar project layout with minor adjustments.
--
-- Doesn't handle conditional includes, and a few other things. Manual
-- editing of the result is still necessary.
--
-- depends on having a POSIX environment with grep, ls, test.
-- arg[1] is also passed to os.execute (system(3)), so it's possible to
-- concatenate arbitrary commands, which may be a bad idea if the input is
-- obtained from an untrusted source and/or the script runs with elevated
-- privileges. e.g., ./lua-stub.lua 'foo; echo bar'

if not arg[1] or #arg[1] == 0 or not os.execute("test -d "..arg[1]) then
  print("directory to Lua source was not supplied as an argument")
  os.exit(1)
end
LUADIR=arg[1]

-- NB: We could try to rip out llex too, but lstate and lcode depends on
-- llex. We could try to rip out lcode, but ldebug depends on lcode, and
-- lapi depends on ldebug. Ripping out llex, lcode, ldebug can be done, but
-- would require patching some functionality which is not as clean as just
-- amalgamating the neccesary files.
HFILES = "cd "..LUADIR.." && ls *.h"
CFILES = "cd "..LUADIR.." && ls *.c | grep -Ev 'lua.c|luac.c'"

-- executes a command from the OS which is expected to return a list of file
-- names fullfilling the pattern %w/_.-
-- side effects: changes io.input
function get_file_listing(command)
  io.input(io.popen(command))
  local headers = {}
  for line in io.lines() do
    for file in line:gmatch("[%w/_.-]+") do
      table.insert(headers, file)
    end
  end
  return headers
end

-- for a sequence of header files, open each header and get the content
-- of each #include statement within ""
-- side effects: changes io.input
function get_header_dependencies(headers)
  local deps = {}
  for _, header in ipairs(headers) do
    io.input(LUADIR.."/"..header)
    for line in io.lines() do
      local m = line:match('#include "([^"]+)"')
      if m ~= nil then
        if deps[header] == nil then
          deps[header] = {m}
        else
          table.insert(deps[header], m)
        end
      end
    end
  end
  return deps
end

-- sort a header dependency index in the correct order
function sort_header_dependencies(index)
  local included = {}
  local headers = {}
  local function f(file)
    if not included[file] then
      local deps = index[file]
      if deps and #deps > 0 then
        for _, child in ipairs(deps) do
          f(child)
        end
      end
      table.insert(headers, file)
      included[file] = true
    end
  end
  for file, _ in pairs(index) do f(file) end
  return headers
end

-- builds an amalgamation in memory and returns it as a string
-- side effects: changes io.input
function amalgamate_files(filenames, includes)
  local result = {}   -- all the lines of the amalgamation
  for _, header in ipairs(filenames) do
    io.input(io.open(LUADIR.."/"..header, "rb"))
    for line in io.lines() do
      local m = line:match('(#include <[^"]+>)')
      if m then
        -- NB: we could include files here that are not listed in
        -- HFILES, CFILES. e.g., we don't want lauxlib.h, but if any file
        -- references it, we will include it anyway, because we probably need
        -- to.
        includes[m] = true
      elseif not line:match('#include "[^"]+"') then
        table.insert(result, line)
      end
    end
  end
  return table.concat(result, "\n"), includes
end

do
  local headers = get_file_listing(HFILES)
  local index = get_header_dependencies(headers)
  local sorted_headers = sort_header_dependencies(index)
  local includes = {}
  local aheader = amalgamate_files(sorted_headers, includes)
  local abody = amalgamate_files(get_file_listing(CFILES), includes)
io.write([[
/*

vim: set tabstop=2 shiftwidth=2 cc=80 expandtab:

try:

musl-gcc -Wl,--build-id=none -Wl,-z,norelro -fno-ident -fno-unwind-tables \
    -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -static -Os -s <file>

$ ls -sh a.out
64K a.out

*/

]])
  print("/*")
  print(unpack(sorted_headers))
  print("*/")
  for inc, _ in pairs(includes) do
    io.write(inc, "\n")
  end
  io.write(aheader, "\n", abody)
end
