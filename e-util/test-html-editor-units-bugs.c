/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <e-util/e-util.h>

#include "test-html-editor-units-utils.h"

#include "test-html-editor-units-bugs.h"

static void
test_bug_726548 (TestFixture *fixture)
{
	gboolean success;
	gchar *text;
	const gchar *expected_plain =
		"aaa\n"
		"   1. a\n"
		"   2. b\n"
		"   3. c\n";

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:aaa\\n\n"
		"action:style-list-number\n"
		"type:a\\nb\\nc\\n\\n\n"
		"seq:C\n"
		"type:ac\n"
		"seq:c\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">aaa</p>"
		"<ol data-evo-paragraph=\"\" style=\"width: 65ch;\">"
		"<li>a</li><li>b</li><li>c</li></ol>"
		"<p style=\"width: 71ch;\"><br></p>" HTML_SUFFIX,
		expected_plain)) {
		g_test_fail ();
		return;
	}

	text = test_utils_get_clipboard_text (FALSE);
	success = test_utils_html_equal (fixture, text, expected_plain);

	if (!success) {
		g_warning ("%s: clipboard Plain text \n---%s---\n does not match expected Plain\n---%s---",
			G_STRFUNC, text, expected_plain);
		g_free (text);
		g_test_fail ();
	} else {
		g_free (text);
	}
}

static void
test_bug_750657 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>\n"
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<p><br></p>\n"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a sub-quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<br>\n"
		"</blockquote>\n"
		"<p>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"</blockquote>\n"
		"<p><br></p>\n"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:uuuSuusD\n",
		HTML_PREFIX
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<p><br></p>\n"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p><br></p>\n"
		"</blockquote>\n"
		"<p>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"</blockquote>\n"
		"<p><br></p>"
		HTML_SUFFIX,
		NULL)) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_760989 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n"
		"type:a\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>\n"
		"One line before quotation<br>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted.</p>\n"
		"</blockquote>\n"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ChcD\n",
		HTML_PREFIX "<p>One line before quotation</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted.</p>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:Cecb\n",
		HTML_PREFIX "<p>One line before quotation</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted</p>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_767903 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:This is the first line:\\n\n"
		"action:style-list-bullet\n"
		"type:First item\\n\n"
		"type:Second item\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">This is the first line:</p>"
		"<ul data-evo-paragraph=\"\" style=\"width: 68ch;\" data-evo-plain-text=\"\">"
		"<li>First item</li><li>Second item<br></li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:uhb\n"
		"undo:undo\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">This is the first line:</p>"
		"<ul data-evo-paragraph=\"\" style=\"width: 68ch;\" data-evo-plain-text=\"\">"
		"<li>First item</li><li>Second item<br></li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_769708 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style id=\"-x-evo-quote-style\" type=\"text/css\">.-x-evo-quoted { -webkit-user-select: none; }</style>"
		"<style id=\"-x-evo-style-a\" type=\"text/css\">a { cursor: text; }</style></head>"
		"<body data-evo-draft=\"\" data-evo-plain-text=\"\" spellcheck=\"true\">"
		"<p data-evo-paragraph=\"\" class=\"\" id=\"-x-evo-input-start\">aaa</p>"
		"<div class=\"-x-evo-signature-wrapper\"><span class=\"-x-evo-signature\" id=\"autogenerated\"><pre>-- <br></pre>"
		"<p data-evo-paragraph=\"\" class=\"\">user &lt;user@no.where&gt;</p>"
		"</span></div></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX_PLAIN "<p>aaa</p><div><span><pre>-- <br></pre>"
		"<p>user &lt;user@no.where&gt;</p>"
		"</span></div>" HTML_SUFFIX,
		"aaa\n"
		"-- \n"
		"user <user@no.where>"))
		g_test_fail ();
}

static void
test_bug_769913 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:ab\n"
		"seq:ltlD\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttllDD\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlDlD\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlllDDD\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlDlDlD\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tb\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttbb\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlbrb\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttbbb\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttllbrbrb\n",
		HTML_PREFIX_PLAIN "<p>ab</p>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_769955 (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines", FALSE);

	/* Use paste action, pretty the same as Ctrl+V */

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:paste\n"
		"seq:ll\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"<a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>"
		HTML_SUFFIX,
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[1] \n"
		"action:paste\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[1] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[1] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[2] \n"
		"action:paste\n"
		"seq:h\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[2] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[2] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[3] \n"
		"action:paste\n"
		"seq:Chc\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[3] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[3] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[4] \n"
		"action:paste\n"
		"seq:l\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[4] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[4] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	/* Use Shift+Insert instead of paste action */

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"seq:Sis\n"
		"seq:ll\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"<a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>"
		HTML_SUFFIX,
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[5] \n"
		"seq:Sis\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[5] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[5] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[6] \n"
		"seq:Sis\n"
		"seq:h\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[6] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[6] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[7] \n"
		"seq:Sis\n"
		"seq:Chc\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[7] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[7] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[8] \n"
		"seq:Sis\n"
		"seq:l\n"
		"action:style-preformat\n",
		HTML_PREFIX_PLAIN "<pre>"
		"[8] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[8] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_770073 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<p><span>the 1st line text</span></p>"
		"<br>"
		"<p><span>the 3rd line text</span></p>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">On Today, User wrote:</p>"
		"<blockquote type=\"cite\">"
		"<p style=\"width: 71ch;\">&gt; the 1st line text</p>"
		"<p style=\"width: 71ch;\">&gt; the 3rd line text</p>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<p><span>the first line text</span></p>"
		"<br>"
		"<p><span>the third line text</span></p>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n",
		HTML_PREFIX "<p>On Today, User wrote:</p>"
		"<blockquote type=\"cite\">"
		"<p><span>the first line text</span></p>"
		"<p><span>the third line text</span></p>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the first line text\n"
		"> the third line text"))
		g_test_fail ();

}

static void
test_bug_770074 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<p><span>the 1st line text</span></p>"
		"<br>"
		"<p><span>the 3rd line text</span></p>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n"
		"seq:n\n"
		"undo:undo\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">On Today, User wrote:</p>"
		"<blockquote type=\"cite\">"
		"<p style=\"width: 71ch;\">&gt; the 1st line text</p>"
		"<p style=\"width: 71ch;\">&gt; the 3rd line text</p>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text"))
		g_test_fail ();
}

void
test_add_html_editor_bug_tests (void)
{
	test_utils_add_test ("/bug/726548", test_bug_726548);
	test_utils_add_test ("/bug/750657", test_bug_750657);
	test_utils_add_test ("/bug/760989", test_bug_760989);
	test_utils_add_test ("/bug/767903", test_bug_767903);
	test_utils_add_test ("/bug/769708", test_bug_769708);
	test_utils_add_test ("/bug/769913", test_bug_769913);
	test_utils_add_test ("/bug/769955", test_bug_769955);
	test_utils_add_test ("/bug/770073", test_bug_770073);
	test_utils_add_test ("/bug/770074", test_bug_770074);
}
