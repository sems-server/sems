mod_regex - regular expressions

This module uses regex(3) for regular expressions.

Regular expressions are referenced by a regex name. They are compiled using
regex.compile action or by adding a line to mod_regex.conf and preloading
mod_regex. The compiled regular expressions can be used with the action
regex.match or the condition regex.match.

Actions:
regex.compile(name, reg_ex)
 Compile a regular expressions in reg_ex to be referenced by name.
 REG_NOSUB | REG_EXTENDED is used.

regex.match(name, match_string)
 Match match_string on regex referenced by name.
 $regex.match is set to 1 if matched, 0 if not matched.

regex.clear(name)
 Clear the regex referenced by name.

Conditions:
 regex.match(name, match_string)
  Match match_string on regex referenced by name.



TODO:
 - implement substring adressing
 - find a better way for $regex.match side-effect
