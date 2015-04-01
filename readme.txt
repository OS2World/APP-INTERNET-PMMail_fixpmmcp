Fix PMMail/2 codepage problems
==============================

Out of frustation for PMMail/2 not using 8 bit transfer encoding and
charset iso-8859-1 codepage by default, and not recognizing "windows"
charset, I wrote yet another command line patch ... use at your own
risk, there are no warranty it won't eat your mail!

What does it do?
================

- If the field "MIME-Version" is not found in the main header,
"MIME-Version: 1.0" is added.

For the main header and each part:

   - If the field "Content-Transfer-Encoding" is not found,
     "Content-Transfer-Encoding: 8bit" is added.
   - If no "Content-Type" field is found,
     "Content-Type: text/plain; charset=iso-8859-1" is added
   - If no charset is defined by "Content-Type" or if the charset
     contains "windows" or "ascii", a Content-Type field with
     charset=iso-8859-1 is added (this could be refined...).

How to use it?
==============

Use it as a filter in PMMail to fix mail you want.  A complex condition that is
verified for all mail that you can use is:

H = ""


Author
======

Samuel Audet <guardia@cam.org>
