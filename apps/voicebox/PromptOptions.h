#ifndef _PROMPT_OPTIONS_H
#define _PROMPT_OPTIONS_H

struct PromptOptions {
  bool has_digits;
  bool digits_right;

  PromptOptions(bool has_digits, 
		bool digits_right)
    : has_digits(has_digits), 
      digits_right(digits_right) { }
  PromptOptions() 
    : has_digits(false), 
      digits_right(false) { }
};

#endif
