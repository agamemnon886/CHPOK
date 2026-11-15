<?xml version="1.0" encoding="UTF-8"?>
<!--
  Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->
<!--
  The dblatex settings of the system requirements documents
  (docs/SRQs/*.xml, built by `make docs`): no table of collaborators and
  no revision history after the title page, the sections of the
  requirements in the table of contents, and no hyphenation, so that no
  word or requirement identifier is broken across lines.
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
  <xsl:param name="doc.collab.show">0</xsl:param>
  <xsl:param name="latex.output.revhistory">0</xsl:param>
  <xsl:param name="doc.lot.show"></xsl:param>
  <xsl:param name="toc.section.depth">1</xsl:param>
  <xsl:param name="doc.section.depth">2</xsl:param>
  <xsl:param name="latex.hyperparam">colorlinks,linkcolor=black</xsl:param>
  <xsl:param name="latex.begindocument">
    <xsl:text>\hyphenpenalty=10000 \exhyphenpenalty=10000&#10;</xsl:text>
    <xsl:text>\sloppy&#10;</xsl:text>
    <!-- No break inside a file name or a literal either. -->
    <xsl:text>\def\UrlBreaks{}&#10;</xsl:text>
    <xsl:text>\def\UrlBigBreaks{}&#10;</xsl:text>
    <xsl:text>\def\dbz{}&#10;</xsl:text>
    <xsl:text>\begin{document}&#10;</xsl:text>
  </xsl:param>
</xsl:stylesheet>
