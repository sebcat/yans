-- vim: shiftwidth=2 tabstop=2 expandtab

function test_url_parse(builder, urlstr, expected)
  local expectedlen = 0
  for _ in pairs(expected) do expectedlen = expectedlen + 1 end
  local actual = builder:parse(urlstr)
  local actuallen = 0
  for name, val in pairs(actual) do
    actuallen = actuallen + 1
    if val ~= expected[name] then
      error(string.format("%s %s: expected:%q actual:%q", urlstr, name,
          expected[name], val))
    end
  end
  if expectedlen ~= actuallen then
    error(string.format("%s expectedlen:%q actuallen:%q", urlstr, expectedlen,
        actuallen))
  end
end

local builder = yans.URLBuilder()
test_url_parse(builder, "", {})
test_url_parse(builder, "//example.com", {
  auth="example.com",
  host="example.com"
})
test_url_parse(builder, "#foo", {fragment="foo"})
test_url_parse(builder, "?foo", {query="foo"})
test_url_parse(builder, "foo", {path="foo"})
test_url_parse(builder, "foo;bar:baz", {path="foo;bar:baz"})
test_url_parse(builder, "foo:", {scheme="foo"})
test_url_parse(builder, "foo://wiie:foo@", {
  scheme="foo",
  auth="wiie:foo@",
  userinfo="wiie:foo"
})
test_url_parse(builder, "foo://wiie:foo@bar", {
  scheme="foo",
  auth="wiie:foo@bar",
  userinfo="wiie:foo",
  host="bar"
})
test_url_parse(builder, "foo://wiie:foo@bar:", {
  scheme="foo",
  auth="wiie:foo@bar:",
  userinfo="wiie:foo",
  host="bar",
  port="",
})
test_url_parse(builder, "foo://wiie:foo@bar:22", {
  scheme="foo",
  auth="wiie:foo@bar:22",
  userinfo="wiie:foo",
  host="bar",
  port="22",
})
test_url_parse(builder, "foo://wiie:foo@bar:xx", {
  scheme="foo",
  auth="wiie:foo@bar:xx",
  userinfo="wiie:foo",
  host="bar",
  port="xx",
})
test_url_parse(builder, "foo://wiie:foo@bar:22/", {
  scheme="foo",
  auth="wiie:foo@bar:22",
  userinfo="wiie:foo",
  host="bar",
  port="22",
  path="/",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo#", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
  fragment="",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo#bar", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
  fragment="bar",
})
test_url_parse(builder, "foo://wiie:foö@[::1%1]:22/?foo#bar", {
  scheme="foo",
  auth="wiie:foö@[::1%1]:22",
  userinfo="wiie:foö",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
  fragment="bar",
})

builder = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_QUERY)
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  fragment="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  fragment="",
})
builder = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_FRAGMENT)
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="",
  fragment="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="",
})
builder = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_QUERY |
    yans.URLFL_REMOVE_EMPTY_FRAGMENT)
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  query="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#foo", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
  fragment="foo",
})
test_url_parse(builder, "foo://wiie:foo@[::1%1]:22/?#", {
  scheme="foo",
  auth="wiie:foo@[::1%1]:22",
  userinfo="wiie:foo",
  host="[::1%1]",
  port="22",
  path="/",
})

function test_url_build(builder, parts, expected)
  local actual = builder:build(parts)
  if actual ~= expected then
    error(string.format("url_build: expected:%q, actual:%q",
        expected, actual))
  end
end

test_url_build(builder, {}, "")
test_url_build(builder, {fragment=""}, "#")
test_url_build(builder, {query=""}, "?")
test_url_build(builder, {fragment="foo"}, "#foo")
test_url_build(builder, {query="foo"}, "?foo")
test_url_build(builder, {path=""}, "")
test_url_build(builder, {path="foo"}, "foo")
test_url_build(builder, {path="/foo"}, "/foo")
test_url_build(builder, {host="example.com"}, "//example.com")
test_url_build(builder, {port=22}, "//:22")
test_url_build(builder, {port="22"}, "//:22")
test_url_build(builder, {userinfo="foo:bar"}, "//foo:bar@")
test_url_build(builder, {auth="example.com"}, "//example.com")
test_url_build(builder, {scheme="http"}, "http:")
test_url_build(builder, {host="example.com", path=""}, "//example.com/")
test_url_build(builder, {auth="example.com", path=""}, "//example.com/")
test_url_build(builder, {host="example.com", port=22}, "//example.com:22")
test_url_build(builder, {host="example.com", port="22"}, "//example.com:22")
test_url_build(builder, {userinfo="foo:bar", host="example.com", port=22},
    "//foo:bar@example.com:22")
-- prefer (userinfo,host,port) over auth
test_url_build(builder, {auth="lel", userinfo="foo:bar", host="example.com",
    port=22}, "//foo:bar@example.com:22")
local ok, _ = pcall(test_url_build, builder, {scheme="foo"}, "...")
if ok then
  error("expected 'invalid scheme' error, was successful")
end

function test_url_inverse(builder, urlstr, expected)
  expected = expected or urlstr
  local actual = builder:build(builder:parse(urlstr))
  if actual ~= expected then
    error(string.format("expected:%q actual:%q", expected, actual))
  end
end

builder = yans.URLBuilder()
test_url_inverse(builder, "")
test_url_inverse(builder, "?")
test_url_inverse(builder, "#")
test_url_inverse(builder, "?foo")
test_url_inverse(builder, "#foo")
test_url_inverse(builder, "somepath")
test_url_inverse(builder, "//@")
test_url_inverse(builder, "//:")
test_url_inverse(builder, "//@:")
test_url_inverse(builder, "//somehost")
test_url_inverse(builder, "//somehost:")
test_url_inverse(builder, "//somehost:20")
test_url_inverse(builder, "//@somehost:20")
test_url_inverse(builder, "//foo@somehost:20")
test_url_inverse(builder, "//somehost/")
test_url_inverse(builder, "//somehost:/")
test_url_inverse(builder, "//somehost:20/")
test_url_inverse(builder, "//@somehost:20/")
test_url_inverse(builder, "//foo@somehost:20/")
test_url_inverse(builder, "http://foo@somehost:20")
test_url_inverse(builder, "http://foo@somehost:20/")
test_url_inverse(builder, "http://foo@somehost:20/foo")
test_url_inverse(builder, "http://foo@somehost:20/foo?")
test_url_inverse(builder, "http://foo@somehost:20/foo#")
test_url_inverse(builder, "http://foo@somehost:20/foo?bar")
test_url_inverse(builder, "http://foo@somehost:20/foo#bar")
test_url_inverse(builder, "http://foo@somehost:20/foo?bar#")
test_url_inverse(builder, "http://foo@somehost:20/foo?bar#baz")

function test_url_normalize(builder, urlstr, expected)
  local actual = builder:normalize(urlstr)
  if actual ~= expected then
    error(string.format("normalize: expected:%q actual:%q", expected, actual))
  end
end

test_url_normalize(builder, "https://www.straße.de/straße",
    "https://www.xn--strae-oqa.de/stra%c3%9fe")
test_url_normalize(builder, "", "")
test_url_normalize(builder, ".", "")
test_url_normalize(builder, "/", "/")
test_url_normalize(builder, "..", "")
test_url_normalize(builder, "../", "")
test_url_normalize(builder, "./", "")
test_url_normalize(builder, "./.", "")
test_url_normalize(builder, "/..", "/")
test_url_normalize(builder, "/../", "/")
test_url_normalize(builder, "././..", "")
test_url_normalize(builder, "././../", "")
test_url_normalize(builder, "././../?foo", "?foo")
test_url_normalize(builder, "foo", "foo")
test_url_normalize(builder, "//example.com", "//example.com/")
test_url_normalize(builder, "http://example.com", "http://example.com/")
test_url_normalize(builder, "http://example.com/", "http://example.com/")
test_url_normalize(builder, "http://example.com/.", "http://example.com/")
test_url_normalize(builder, "http://example.com/./", "http://example.com/")
test_url_normalize(builder, "http://example.com/./foo",
    "http://example.com/foo")
test_url_normalize(builder, "http://example.com/./foo/",
    "http://example.com/foo/")
test_url_normalize(builder, "http://example.com/..", "http://example.com/")
test_url_normalize(builder, "http://example.com/../", "http://example.com/")
test_url_normalize(builder, "http://example.com/foo/..", "http://example.com/")
test_url_normalize(builder, "http://example.com/foo/../bar",
    "http://example.com/bar")
test_url_normalize(builder, "http://example.com/foo/../bar/",
    "http://example.com/bar/")
test_url_normalize(builder, "http://example.com/föö/../bar/",
    "http://example.com/bar/")
test_url_normalize(builder, "http://example.com/foo/../bör/",
    "http://example.com/b%c3%b6r/")
test_url_normalize(builder, "http://example.com/foo/../bar/?q=?&bar=öh",
    "http://example.com/bar/?q=?&bar=%c3%b6h")

function test_url_resolve(builder, base, ref, expected)
  local actual = builder:resolve(base, ref)
  if actual ~= expected then
    error(string.format("resolve: expected:%q actual:%q", expected, actual))
  end
end
builder = yans.URLBuilder(yans.URLFL_REMOVE_EMPTY_QUERY |
    yans.URLFL_REMOVE_EMPTY_FRAGMENT)
test_url_resolve(builder,"http://example.com/", "http://www.example.aom",
    "http://www.example.aom/")
test_url_resolve(builder,"", "", "")
test_url_resolve(builder,"foo", "/bar", "/bar")
test_url_resolve(builder,"foo", "?bar", "foo?bar")
test_url_resolve(builder,"/foo", "?bar", "/foo?bar")
test_url_resolve(builder,"/foo", "#bar", "/foo#bar")
--base is not normalized in url_resolve */
test_url_resolve(builder,"http://example.com", "", "http://example.com")
test_url_resolve(builder,"http://example.com/", "", "http://example.com/")
test_url_resolve(builder,"http://example.com/", "?", "http://example.com/")
test_url_resolve(builder,"http://example.com/", "#", "http://example.com/")
test_url_resolve(builder,"http://example.com/", "#bar",
    "http://example.com/#bar")
test_url_resolve(builder,"http://example.com/", "?#foo",
    "http://example.com/#foo")
test_url_resolve(builder,"http://example.com/", "?foo",
    "http://example.com/?foo")
test_url_resolve(builder,"http://example.com/foo", "?bar",
    "http://example.com/foo?bar")
test_url_resolve(builder,"http://example.com/foo", "?bar#baz",
    "http://example.com/foo?bar#baz")
test_url_resolve(builder,"http://example.com/foo", "/",
    "http://example.com/")
test_url_resolve(builder,"http://example.com/foo", "/..",
    "http://example.com/")
test_url_resolve(builder,"http://example.com/foo/", "/",
    "http://example.com/")
test_url_resolve(builder,"http://example.com/foo/", "/..",
    "http://example.com/")
test_url_resolve(builder,"http://example.com/foo/", "/bar",
    "http://example.com/bar")
test_url_resolve(builder,"http://example.com/foo/", "/bar/",
    "http://example.com/bar/")
test_url_resolve(builder,"http://example.com/", "http://www.example.aom",
    "http://www.example.aom/")
test_url_resolve(builder,"http://example.com/", "http://www.example.bom/",
    "http://www.example.bom/")
test_url_resolve(builder,"http://example.com/", "//www.example.com/",
    "http://www.example.com/")
test_url_resolve(builder,"//example.com/", "//www.example.com/",
    "//www.example.com/")
test_url_resolve(builder,"//example.com/", "//www.example.com",
    "//www.example.com/")
test_url_resolve(builder,"http://example.com/", "bar",
    "http://example.com/bar")
test_url_resolve(builder,"http://example.com/foo", "bar",
    "http://example.com/bar")
test_url_resolve(builder,"http://example.com/foo/", "bar",
    "http://example.com/foo/bar")
test_url_resolve(builder,"//example.com/", "bar",
    "//example.com/bar")
test_url_resolve(builder,"//example.com/foo", "bar",
    "//example.com/bar")
test_url_resolve(builder,"//example.com/foo/", "bar/",
    "//example.com/foo/bar/")
test_url_resolve(builder,"https://example.com/foo/", "bar/?",
    "https://example.com/foo/bar/")
test_url_resolve(builder,"https://example.com/foo/", "bar/?#",
    "https://example.com/foo/bar/")
test_url_resolve(builder,"https://example.com/foo/", "bar?baz",
    "https://example.com/foo/bar?baz")
test_url_resolve(builder,"https://example.com/foo/", "bar/?baz",
    "https://example.com/foo/bar/?baz")
test_url_resolve(builder,"https://example.com/foo/", "bar#baz",
    "https://example.com/foo/bar#baz")
test_url_resolve(builder,"https://example.com/foo/", "bar/#baz",
    "https://example.com/foo/bar/#baz")
test_url_resolve(builder,"https://example.com/", "//www.example.com/",
    "https://www.example.com/")
test_url_resolve(builder,"https://example.com/", "//www.example.com/?",
    "https://www.example.com/")
test_url_resolve(builder,"https://example.com/", "//www.example.com/?#",
    "https://www.example.com/")
test_url_resolve(builder,"https://example.com/", "//www.example.com/?foo#",
    "https://www.example.com/?foo")
test_url_resolve(builder,"https://example.com/", "//www.example.com/?foo#bar",
    "https://www.example.com/?foo#bar")
test_url_resolve(builder,"https://example.com/", "//www.example.com/?foö#bar",
    "https://www.example.com/?fo%c3%b6#bar")
test_url_resolve(builder,"https:", "//www.straße.de/straße",
    "https://www.xn--strae-oqa.de/stra%c3%9fe")
