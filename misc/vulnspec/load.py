#!/usr/bin/env python3
# Copyright (c) 2019 Sebastian Cato
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import json
import gzip
import sys

class VulnspecBuilder:
  def __init__(self, isym = "  "):
    self.isym = isym
    self.level = 0
    self.nalphavers = set((
      "openssl/openssl",
      "openbsd/openssh",
    ))
    self.vendprod_map = {
      "php/php":                   "php/php",
      "drupal/drupal":             "drupal/drupal",
      "openssl/openssl":           "openssl/openssl",
      "openssl_project/openssl":   "openssl/openssl",
      "wordpress/wordpress":       "wordpress/wordpress",
      "apache/apache":             "apache/http_server",
      "apache/http_server":        "apache/http_server",
      "apache/apache_http_server": "apache/http_server",
      "nginx/nginx":               "nginx/nginx",
      "igor_sysoev/nginx":         "nginx/nginx",
      "openbsd/openssh":           "openbsd/openssh",
      "beasts/vsftpd":             "beasts/vsftpd",
      "exit/exim":                 "exim/exim",
      "university_of_cambridge/exim": "exim/exim",
      "postfix/postfix":           "postfix/postfix",
      "microsoft/iis":             "microsoft/iis",
      "lighttpd/lighttpd":         "lighttpd/lighttpd",
      "eclipse/jetty":             "jetty/jetty",
      "mortbay/jetty":             "jetty/jetty",
      "jetty/jetty":               "jetty/jetty",
      "jetty/jetty_http_server":   "jetty/jetty",
    }



  def enter(self):
    self.level += 1

  def exit(self):
    self.level -= 1

  def indent(self):
    return "\n{}".format(self.isym * self.level)

  def escape_string(self, s):
    return s.translate(str.maketrans({"\\":  "\\\\", '"': '\\"'}))

  def make_cmp(self, op, vendprod, version, nalphavers = False):
    if nalphavers:
      return '(nalpha ({} "{}" "{}"))'.format(op, vendprod, version)
    else:
      return '({} "{}" "{}")'.format(op, vendprod, version)


  def process_cpe_match(self, node):
    if not node["vulnerable"]:
      return ""

    cpe_elems = node["cpe23Uri"].split(":")
    vendor = cpe_elems[3]
    product = cpe_elems[4]
    vendprod = self.escape_string("{}/{}".format(vendor, product))
    version = self.escape_string(cpe_elems[5])
    if not vendprod in self.vendprod_map:
      return ""
    else:
      vendprod = self.vendprod_map[vendprod]
    nalphavers = True if vendprod in self.nalphavers else False

    cmps = []
    if "versionStartExcluding" in node:
      v = node["versionStartExcluding"]
      cmps.append(self.make_cmp('>', vendprod, v, nalphavers))
    if "versionStartIncluding" in node:
      v = node["versionStartIncluding"]
      cmps.append(self.make_cmp('>=', vendprod, v, nalphavers))
    if "versionEndExcluding" in node:
      v = node["versionEndExcluding"]
      cmps.append(self.make_cmp('<', vendprod, v, nalphavers))
    if "versionEndIncluding" in node:
      v = node["versionEndIncluding"]
      cmps.append(self.make_cmp('<=', vendprod, v, nalphavers))
    if version != "*" and version != "-":
      cmps.append(self.make_cmp('=', vendprod, version, nalphavers))

    self.enter()
    if len(cmps) == 0:
      res = ""
    elif len(cmps) == 1:
      res = "{}{}".format(self.indent(), cmps[0])
    else:
      self.enter()
      children = ["{}{}".format(self.indent(), x) for x in cmps]
      nodes = "".join(children)
      self.exit()
      res = '{}(^{})'.format(self.indent(), nodes)
    self.exit()

    if self.is_unique(res):
      return res
    else:
      return ""

  def process_cpe_match_node(self, node):
    res = None
    children = [self.process_cpe_match(child) for child in node["cpe_match"]]
    res = "".join(children)
    return res
  
  def process_and_node(self, node):
    self.enter()
    cpe_match_str = ""
    if "cpe_match" in node:
      cpe_match_str = self.process_cpe_match_node(node)

    children_str = ""
    if "children" in node:
      children_str = self.process_nodes(node["children"])
    self.exit()

    if len(cpe_match_str) == 0 and len(children_str) == 0:
      return ""

    self.enter()
    res = "{}(^{}{})".format(self.indent(), cpe_match_str, children_str)
    self.exit()
    return res
  
  def process_or_node(self, node):
    res = None
    if not "cpe_match" in node or len(node["cpe_match"]) == 0:
      return ""
    elif "children" in node:
      raise Exception("Child nodes in or node")

    self.enter()
    matches = self.process_cpe_match_node(node)
    if len(matches) > 0:
      res =  "{}(v{})".format(self.indent(), matches)
    else:
      res = ""
    self.exit()
    return res
  
  def process_node(self, node):
    if node["operator"] == "OR":
      return self.process_or_node(node)
    elif node["operator"] == "AND":
      self.push_dedup()
      res = self.process_and_node(node)
      self.pop_dedup()
      return res
    else:
      raise Exception("Unknown node: {}".format(node["operator"]))

  def process_nodes(self, nodes):
    children = [self.process_node(node) for node in nodes]
    return "".join(children)

  def or_nodes_tl(self, nodes):
    self.enter()
    children = [self.process_node(node) for node in nodes]
    children = "".join(children)
    if len(children) > 0:
      res = "{}(v{})".format(self.indent(), children)
    else:
      res = ""
    self.exit()
    return res  
  
  def process_nodes_tl(self, nodes):
    if len(nodes) > 1:
      return self.or_nodes_tl(nodes)
    else:
      return self.process_node(nodes[0])

  def push_dedup(self):
    self.dedup.append(set())

  def pop_dedup(self):
    return self.dedup.pop()

  def init_dedup(self):
    self.dedup = [set()]

  def is_unique(self, s):
    if s in self.dedup[-1]:
      return False
    else:
      self.dedup[-1].add(s)
      return True
  
  def process_item(self, item):
    # skip CVEs without any vulnerable configuration (REJECTEDs &c)
    if len(item["configurations"]["nodes"]) == 0:
      return

    self.init_dedup()

    cve_id = self.escape_string(item["cve"]["CVE_data_meta"]["ID"])
    en_description = ""
    for description in item["cve"]["description"]["description_data"]:
      if description["lang"] == "en":
        en_description = self.escape_string(description["value"])
        break

    cvss2_base = -1
    cvss3_base = -1

    try:
      cvss2_base = item["impact"]["baseMetricV2"]["cvssV2"]["baseScore"]
    except:
      pass

    try:
      cvss3_base = item["impact"]["baseMetricV3"]["cvssV3"]["baseScore"]
    except:
      pass

    vexpr = self.process_nodes_tl(item["configurations"]["nodes"])
    if len(vexpr) > 0:
      return '(cve "{}" {:.2f} {:.2f} "{}"{})'.format(cve_id, cvss2_base, cvss3_base, en_description, vexpr)

if __name__ == '__main__':
  filename = sys.argv[1]
  b = VulnspecBuilder()
  with gzip.open(filename, "rb") as f:
    data = json.load(f)
    for item in data["CVE_Items"]:
      item = b.process_item(item)
      if item is not None:
        print("{}\n".format(item))
