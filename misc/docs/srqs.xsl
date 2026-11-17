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

  Modified by Kirill Brazhnikov, 2026-11-16: the Traceability of a
  requirement (a list of long identifiers) is ragged right, so that it
  doesn't stick out of the margin, and lists its identifiers one per
  line, bold and dark green, each a link to its place in the function
  documentation; the identifiers of the requirements are dark red. The
  HLRs of a Traceability are plain identifiers (no phrase elements, so
  that long ones fit in a line), and the document of each one comes from
  the list that make writes from the C files (jetos.hlrdocs).
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
    <!-- The colors of the identifiers: SRQ dark red, HLR dark green
         (the same as in Doxyfile.common). -->
    <xsl:text>\usepackage{xcolor}&#10;</xsl:text>
    <xsl:text>\definecolor{jetossrq}{RGB}{139,0,0}&#10;</xsl:text>
    <xsl:text>\definecolor{jetoshlr}{RGB}{0,100,0}&#10;</xsl:text>
    <xsl:text>\begin{document}&#10;</xsl:text>
  </xsl:param>
  <!-- The Traceability of a requirement: ragged right, its identifiers
       in bold. -->
  <xsl:template match="formalpara[title='Traceability']">
    <xsl:text>{\raggedright&#10;</xsl:text>
    <xsl:if test="not(starts-with(normalize-space(para),
                  'Not applicable'))">
      <xsl:text>\bfseries&#10;</xsl:text>
    </xsl:if>
    <xsl:apply-imports/>
    <xsl:text>\par}&#10;</xsl:text>
  </xsl:template>
  <!-- An SRQ identifier (the title of a requirement): dark red. -->
  <xsl:template match="phrase[@role='srq']">
    <xsl:text>\textcolor{jetossrq}{</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>}</xsl:text>
  </xsl:template>
  <!-- The list of the HLRs and their documents, written by make (the
       absolute path of build/docs/<book>/hlrdocs.xml):
       <hlrs><h id="HLR_..." d="kernel"/>...</hlrs>, d being kernel,
       boards, shared or hypervisor. -->
  <xsl:param name="jetos.hlrdocs"/>
  <xsl:variable name="jetos.hlrs"
                select="document($jetos.hlrdocs)/hlrs/h"/>
  <!-- The HLRs of a Traceability (identifiers separated by white space,
       unless it is "Not applicable"): each on a line of its own, like
       the items of a list. -->
  <xsl:template match="formalpara[title='Traceability']/para[not(
                       starts-with(normalize-space(.), 'Not applicable'))]
                       /text()">
    <xsl:call-template name="jetos.hlrs">
      <xsl:with-param name="ids" select="concat(normalize-space(.), ' ')"/>
    </xsl:call-template>
  </xsl:template>
  <!-- The identifiers of $ids (each followed by a space): dark green,
       bold, each a link to the HLR in its document, next to this one in
       docs/ (plain text if the list doesn't have it: make docs-trace
       reports it). -->
  <xsl:template name="jetos.hlrs">
    <xsl:param name="ids"/>
    <xsl:variable name="id" select="substring-before($ids, ' ')"/>
    <xsl:if test="$id != ''">
      <xsl:variable name="doc" select="$jetos.hlrs[@id = $id]/@d"/>
      <xsl:text>\newline\hspace*{1.5em}</xsl:text>
      <xsl:if test="$doc">
        <xsl:value-of select="concat('\href{', $doc, '.pdf\#', $id, '}')"/>
      </xsl:if>
      <xsl:text>{\textcolor{jetoshlr}{\textbf{</xsl:text>
      <!-- Identifiers have only capitals, digits and underscores. -->
      <xsl:call-template name="jetos.underscores">
        <xsl:with-param name="s" select="$id"/>
      </xsl:call-template>
      <xsl:text>}}}</xsl:text>
      <xsl:call-template name="jetos.hlrs">
        <xsl:with-param name="ids" select="substring-after($ids, ' ')"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>
  <!-- $s with each underscore written \_ for LaTeX. -->
  <xsl:template name="jetos.underscores">
    <xsl:param name="s"/>
    <xsl:choose>
      <xsl:when test="contains($s, '_')">
        <xsl:value-of select="concat(substring-before($s, '_'), '\_')"/>
        <xsl:call-template name="jetos.underscores">
          <xsl:with-param name="s" select="substring-after($s, '_')"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$s"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>
