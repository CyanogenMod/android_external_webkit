/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

// Magic pretend-to-be-a-chromium-build flags
#undef WEBKIT_IMPLEMENTATION
#undef LOG

#include "content/address_detector.h"

#include <bitset>

#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "Settings.h"
#include "WebString.h"

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace {

// Prefix used for geographical address intent URIs.
static const char kAddressSchemaPrefix[] = "geo:0,0?q=";

// Maximum text length to be searched for address detection.
static const size_t kMaxAddressLength = 500;

// Minimum number of words in an address after the house number
// before a state is expected to be found.
// A value too high can miss short addresses.
const size_t kMinAddressWords = 3;

// Maximum number of words allowed in an address between the house number
// and the state, both not included.
const size_t kMaxAddressWords = 12;

// Maximum number of lines allowed in an address between the house number
// and the state, both not included.
const size_t kMaxAddressLines = 5;

// Maximum length allowed for any address word between the house number
// and the state, both not included.
const size_t kMaxAddressNameWordLength = 25;

// Maximum number of words after the house number in which the location name
// should be found.
const size_t kMaxLocationNameDistance = 4;

// Number of digits for a valid zip code.
const size_t kZipDigits = 5;

// Number of digits for a valid zip code in the Zip Plus 4 format.
const size_t kZipPlus4Digits = 9;

// Maximum number of digits of a house number, including possible hyphens.
const size_t kMaxHouseDigits = 5;

// Additional characters used as new line delimiters.
const char16 kNewlineDelimiters[] = {
  ',',
  '*',
  0x2022,  // Unicode bullet
  0,
};

char16 SafePreviousChar(const string16::const_iterator& it,
    const string16::const_iterator& begin) {
  if (it == begin)
    return ' ';
  return *(it - 1);
}

char16 SafeNextChar(const string16::const_iterator& it,
    const string16::const_iterator& end) {
  if (it == end)
    return ' ';
  return *(it + 1);
}

bool WordLowerCaseEqualsASCII(string16::const_iterator word_begin,
    string16::const_iterator word_end, const char* ascii_to_match) {
  for (string16::const_iterator it = word_begin; it != word_end;
      ++it, ++ascii_to_match) {
    if (!*ascii_to_match || base::ToLowerASCII(*it) != *ascii_to_match)
      return false;
  }
  return *ascii_to_match == 0 || *ascii_to_match == ' ';
}

bool LowerCaseEqualsASCIIWithPlural(string16::const_iterator word_begin,
    string16::const_iterator word_end, const char* ascii_to_match,
    bool allow_plural) {
  for (string16::const_iterator it = word_begin; it != word_end;
      ++it, ++ascii_to_match) {
    if (!*ascii_to_match && allow_plural && *it == 's' && it + 1 == word_end)
      return true;

    if (!*ascii_to_match || base::ToLowerASCII(*it) != *ascii_to_match)
      return false;
  }
  return *ascii_to_match == 0;
}

}  // anonymous namespace


AddressDetector::AddressDetector() {
}

AddressDetector::~AddressDetector() {
}

std::string AddressDetector::GetContentText(const WebKit::WebRange& range) {
  // Get the address and replace unicode bullets with commas.
  string16 address_16 = CollapseWhitespace(range.toPlainText(), false);
  std::replace(address_16.begin(), address_16.end(),
      static_cast<char16>(0x2022), static_cast<char16>(','));
  return UTF16ToUTF8(address_16);
}

GURL AddressDetector::GetIntentURL(const std::string& content_text) {
  return GURL(kAddressSchemaPrefix +
      EscapeQueryParamValue(content_text, true));
}

size_t AddressDetector::GetMaximumContentLength() {
  return kMaxAddressLength;
}

bool AddressDetector::IsEnabled(const WebKit::WebHitTestInfo& hit_test) {
  WebCore::Settings* settings = GetSettings(hit_test);
  return settings && settings->formatDetectionAddress();
}

bool AddressDetector::FindContent(const string16::const_iterator& begin,
    const string16::const_iterator& end, size_t* start_pos, size_t* end_pos) {
  HouseNumberParser house_number_parser;

  // Keep going through the input string until a potential house number is
  // detected. Start tokenizing the following words to find a valid
  // street name within a word range. Then, find a state name followed
  // by a valid zip code for that state. Also keep a look for any other
  // possible house numbers to continue from in case of no match and for
  // state names not followed by a zip code (e.g. New York, NY 10000).
  const string16 newline_delimiters = kNewlineDelimiters;
  const string16 delimiters = kWhitespaceUTF16 + newline_delimiters;
  for (string16::const_iterator it = begin; it != end; ) {
    Word house_number;
    if (!house_number_parser.Parse(it, end, &house_number))
      return false;

    String16Tokenizer tokenizer(house_number.end, end, delimiters);
    tokenizer.set_options(String16Tokenizer::RETURN_DELIMS);

    std::vector<Word> words;
    words.push_back(house_number);

    bool found_location_name = false;
    bool continue_on_house_number = true;
    size_t next_house_number_word = 0;
    size_t num_lines = 1;

    // Don't include the house number in the word count.
    size_t next_word = 1;
    for (; next_word <= kMaxAddressWords + 1; ++next_word) {

      // Extract a new word from the tokenizer.
      if (next_word == words.size()) {
        do {
          if (!tokenizer.GetNext())
            return false;

          // Check the number of address lines.
          if (tokenizer.token_is_delim() && newline_delimiters.find(
              *tokenizer.token_begin()) != string16::npos) {
            ++num_lines;
          }
        } while (tokenizer.token_is_delim());

        if (num_lines > kMaxAddressLines)
          break;

        words.push_back(Word(tokenizer.token_begin(), tokenizer.token_end()));
      }

      // Check the word length. If too long, don't try to continue from
      // the next house number as no address can hold this word.
      const Word& current_word = words[next_word];
      DCHECK_GT(std::distance(current_word.begin, current_word.end), 0);
      size_t current_word_length = std::distance(
          current_word.begin, current_word.end);
      if (current_word_length > kMaxAddressNameWordLength) {
        continue_on_house_number = false;
        break;
      }

      // Check if the new word is a valid house number.
      // This is used to properly resume parsing in case the maximum number
      // of words is exceeded.
      if (next_house_number_word == 0 &&
          house_number_parser.Parse(current_word.begin, current_word.end, NULL)) {
        next_house_number_word = next_word;
        continue;
      }

      // Look for location names in the words after the house number.
      // A range limitation is introduced to avoid matching
      // anything that starts with a number before a legitimate address.
      if (next_word <= kMaxLocationNameDistance &&
          IsValidLocationName(current_word)) {
        found_location_name = true;
        continue;
      }

      // Don't count the house number.
      if (next_word > kMinAddressWords) {
        // Looking for the state is likely to add new words to the list while
        // checking for multi-word state names.
        size_t state_first_word = next_word;
        size_t state_last_word, state_index;
        if (FindStateStartingInWord(&words, state_first_word, &state_last_word,
            &tokenizer, &state_index)) {

          // A location name should have been found at this point.
          if (!found_location_name)
            break;

          // Explicitly exclude "et al", as "al" is a valid state code.
          if (current_word_length == 2 && words.size() > 2) {
            const Word& previous_word = words[state_first_word - 1];
            if (previous_word.end - previous_word.begin == 2 &&
                LowerCaseEqualsASCII(previous_word.begin, previous_word.end,
                    "et") &&
                LowerCaseEqualsASCII(current_word.begin, current_word.end,
                    "al"))
              break;
          }

          // Extract one more word from the tokenizer if not already available.
          size_t zip_word = state_last_word + 1;
          if (zip_word == words.size()) {
            do {
              if (!tokenizer.GetNext()) {
                // Zip is optional
                *start_pos = words[0].begin - begin;
                *end_pos = words[state_last_word].end - begin;
                return true;
              }
            } while (tokenizer.token_is_delim());
            words.push_back(Word(tokenizer.token_begin(),
                tokenizer.token_end()));
          }

          // Check the parsing validity and state range of the zip code.
          next_word = state_last_word;
          if (!IsZipValid(words[zip_word], state_index))
            continue;

          *start_pos = words[0].begin - begin;
          *end_pos = words[zip_word].end - begin;
          return true;
        }
      }
    }

    // Avoid skipping too many words because of a non-address number
    // at the beginning of the contents to parse.
    if (continue_on_house_number && next_house_number_word > 0) {
      it = words[next_house_number_word].begin;
    } else {
      DCHECK(!words.empty());
      next_word = std::min(next_word, words.size() - 1);
      it = words[next_word].end;
    }
  }

  return false;
}

bool AddressDetector::HouseNumberParser::IsPreDelimiter(
    char16 character) {
  return character == ':' || IsPostDelimiter(character);
}

bool AddressDetector::HouseNumberParser::IsPostDelimiter(
    char16 character) {
  return IsWhitespace(character) || strchr(",\"'", character);
}

void AddressDetector::HouseNumberParser::RestartOnNextDelimiter() {
  ResetState();
  for (; it_ != end_ && !IsPreDelimiter(*it_); ++it_) {}
}

void AddressDetector::HouseNumberParser::AcceptChars(size_t num_chars) {
  size_t offset = std::min(static_cast<size_t>(std::distance(it_, end_)),
      num_chars);
  it_ += offset;
  result_chars_ += offset;
}

void AddressDetector::HouseNumberParser::SkipChars(size_t num_chars) {
  it_ += std::min(static_cast<size_t>(std::distance(it_, end_)), num_chars);
}

void AddressDetector::HouseNumberParser::ResetState() {
  num_digits_ = 0;
  result_chars_ = 0;
}

bool AddressDetector::HouseNumberParser::CheckFinished(Word* word) const {
  // There should always be a number after a hyphen.
  if (result_chars_ == 0 || SafePreviousChar(it_, begin_) == '-')
    return false;

  if (word) {
    word->begin = it_ - result_chars_;
    word->end = it_;
  }
  return true;
}

bool AddressDetector::HouseNumberParser::Parse(
    const string16::const_iterator& begin,
    const string16::const_iterator& end, Word* word) {
  it_ = begin_ = begin;
  end_ = end;
  ResetState();

  // Iterations only used as a fail-safe against any buggy infinite loops.
  size_t iterations = 0;
  size_t max_iterations = end - begin + 1;
  for (; it_ != end_ && iterations < max_iterations; ++iterations) {

    // Word finished case.
    if (IsPostDelimiter(*it_)) {
      if (CheckFinished(word))
        return true;
      else if (result_chars_)
        ResetState();

      SkipChars(1);
      continue;
    }

    // More digits. There should be no more after a letter was found.
    if (IsAsciiDigit(*it_)) {
      if (num_digits_ >= kMaxHouseDigits) {
        RestartOnNextDelimiter();
      } else {
        AcceptChars(1);
        ++num_digits_;
      }
      continue;
    }

    if (IsAsciiAlpha(*it_)) {
      // Handle special case 'one'.
      if (result_chars_ == 0) {
        if (it_ + 3 <= end_ && LowerCaseEqualsASCII(it_, it_ + 3, "one"))
          AcceptChars(3);
        else
          RestartOnNextDelimiter();
        continue;
      }

      // There should be more than 1 character because of result_chars.
      DCHECK_GT(result_chars_, 0U);
      DCHECK_NE(it_, begin_);
      char16 previous = SafePreviousChar(it_, begin_);
      if (IsAsciiDigit(previous)) {
        // Check cases like '12A'.
        char16 next = SafeNextChar(it_, end_);
        if (IsPostDelimiter(next)) {
          AcceptChars(1);
          continue;
        }

        // Handle cases like 12a, 1st, 2nd, 3rd, 7th.
        if (IsAsciiAlpha(next)) {
          char16 last_digit = previous;
          char16 first_letter = base::ToLowerASCII(*it_);
          char16 second_letter = base::ToLowerASCII(next);
          bool is_teen = SafePreviousChar(it_ - 1, begin_) == '1' &&
              num_digits_ == 2;

          switch (last_digit - '0') {
          case 1:
            if ((first_letter == 's' && second_letter == 't') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 2:
            if ((first_letter == 'n' && second_letter == 'd') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 3:
            if ((first_letter == 'r' && second_letter == 'd') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 0:
            // Explicitly exclude '0th'.
            if (num_digits_ == 1)
              break;

          case 4:
          case 5:
          case 6:
          case 7:
          case 8:
          case 9:
            if (first_letter == 't' && second_letter == 'h') {
              AcceptChars(2);
              continue;
            }
            break;

          default:
            NOTREACHED();
          }
        }
      }

      RestartOnNextDelimiter();
      continue;
    }

    if (*it_ == '-' && num_digits_ > 0) {
      AcceptChars(1);
      ++num_digits_;
      continue;
    }

    RestartOnNextDelimiter();
    SkipChars(1);
  }

  if (iterations >= max_iterations)
    return false;

  return CheckFinished(word);
}

bool AddressDetector::FindStateStartingInWord(WordList* words,
    size_t state_first_word, size_t* state_last_word,
    String16Tokenizer* tokenizer, size_t* state_index) {

  // Bitmasks containing the allowed suffixes for 2-letter state codes.
  static const int state_two_letter_suffix[23] = {
    0x02060c00,  // A followed by: [KLRSZ].
    0x00000000,  // B.
    0x00084001,  // C followed by: [AOT].
    0x00000014,  // D followed by: [CE].
    0x00000000,  // E.
    0x00001800,  // F followed by: [LM].
    0x00100001,  // G followed by: [AU].
    0x00000100,  // H followed by: [I].
    0x00002809,  // I followed by: [ADLN].
    0x00000000,  // J.
    0x01040000,  // K followed by: [SY].
    0x00000001,  // L followed by: [A].
    0x000ce199,  // M followed by: [ADEHINOPST].
    0x0120129c,  // N followed by: [CDEHJMVY].
    0x00020480,  // O followed by: [HKR].
    0x00420001,  // P followed by: [ARW].
    0x00000000,  // Q.
    0x00000100,  // R followed by: [I].
    0x0000000c,  // S followed by: [CD].
    0x00802000,  // T followed by: [NX].
    0x00080000,  // U followed by: [T].
    0x00080101,  // V followed by: [AIT].
    0x01200101   // W followed by: [AIVY].
  };

  // Accumulative number of states for the 2-letter code indexed by the first.
  static const int state_two_letter_accumulative[24] = {
     0,  5,  5,  8, 10, 10, 12, 14,
    15, 19, 19, 21, 22, 32, 40, 43,
    46, 46, 47, 49, 51, 52, 55, 59
  };

  // State names sorted alphabetically with their lengths.
  // There can be more than one possible name for a same state if desired.
  static const struct StateNameInfo {
    const char* string;
    char first_word_length;
    char length;
    char state_index; // Relative to two-character code alphabetical order.
  } state_names[59] = {
    { "alabama", 7, 7, 1 }, { "alaska", 6, 6, 0 },
    { "american samoa", 8, 14, 3 }, { "arizona", 7, 7, 4 },
    { "arkansas", 8, 8, 2 },
    { "california", 10, 10, 5 }, { "colorado", 8, 8, 6 },
    { "connecticut", 11, 11, 7 }, { "delaware", 8, 8, 9 },
    { "district of columbia", 8, 20, 8 },
    { "federated states of micronesia", 9, 30, 11 }, { "florida", 7, 7, 10 },
    { "guam", 4, 4, 13 }, { "georgia", 7, 7, 12 },
    { "hawaii", 6, 6, 14 },
    { "idaho", 5, 5, 16 }, { "illinois", 8, 8, 17 }, { "indiana", 7, 7, 18 },
    { "iowa", 4, 4, 15 },
    { "kansas", 6, 6, 19 }, { "kentucky", 8, 8, 20 },
    { "louisiana", 9, 9, 21 },
    { "maine", 5, 5, 24 }, { "marshall islands", 8, 16, 25 },
    { "maryland", 8, 8, 23 }, { "massachusetts", 13, 13, 22 },
    { "michigan", 8, 8, 26 }, { "minnesota", 9, 9, 27 },
    { "mississippi", 11, 11, 30 }, { "missouri", 8, 8, 28 },
    { "montana", 7, 7, 31 },
    { "nebraska", 8, 8, 34 }, { "nevada", 6, 6, 38 },
    { "new hampshire", 3, 13, 35 }, { "new jersey", 3, 10, 36 },
    { "new mexico", 3, 10, 37 }, { "new york", 3, 8, 39 },
    { "north carolina", 5, 14, 32 }, { "north dakota", 5, 12, 33 },
    { "northern mariana islands", 8, 24, 29 },
    { "ohio", 4, 4, 40 }, { "oklahoma", 8, 8, 41 }, { "oregon", 6, 6, 42 },
    { "palau", 5, 5, 45 }, { "pennsylvania", 12, 12, 43 },
    { "puerto rico", 6, 11, 44 },
    { "rhode island", 5, 5, 46 },
    { "south carolina", 5, 14, 47 }, { "south dakota", 5, 12, 48 },
    { "tennessee", 9, 9, 49 }, { "texas", 5, 5, 50 },
    { "utah", 4, 4, 51 },
    { "vermont", 7, 7, 54 }, { "virgin islands", 6, 14, 53 },
    { "virginia", 8, 8, 52 },
    { "washington", 10, 10, 55 }, { "west virginia", 4, 13, 57 },
    { "wisconsin", 9, 9, 56 }, { "wyoming", 7, 7, 58 }
  };

  // Accumulative number of states for sorted names indexed by the first letter.
  // Required a different one since there are codes that don't share their
  // first letter with the name of their state (MP = Northern Mariana Islands).
  static const int state_names_accumulative[24] = {
     0,  5,  5,  8, 10, 10, 12, 14,
    15, 19, 19, 21, 22, 31, 40, 43,
    46, 46, 47, 49, 51, 52, 55, 59
  };

  DCHECK_EQ(state_names_accumulative[arraysize(state_names_accumulative) - 1],
      static_cast<int>(ARRAYSIZE_UNSAFE(state_names)));

  const Word& first_word = words->at(state_first_word);
  int length = first_word.end - first_word.begin;
  if (length < 2 || !IsAsciiAlpha(*first_word.begin))
    return false;

  // No state names start with x, y, z.
  char16 first_letter = base::ToLowerASCII(*first_word.begin);
  if (first_letter > 'w')
    return false;

  DCHECK(first_letter >= 'a');
  int first_index = first_letter - 'a';

  // Look for two-letter state names.
  if (length == 2 && IsAsciiAlpha(*(first_word.begin + 1))) {
    char16 second_letter = base::ToLowerASCII(*(first_word.begin + 1));
    DCHECK(second_letter >= 'a');

    int second_index = second_letter - 'a';
    if (!(state_two_letter_suffix[first_index] & (1 << second_index)))
      return false;

    std::bitset<32> previous_suffixes = state_two_letter_suffix[first_index] &
        ((1 << second_index) - 1);
    *state_last_word = state_first_word;
    *state_index = state_two_letter_accumulative[first_index] +
        previous_suffixes.count();
    return true;
  }

  // Look for full state names by their first letter. Discard by length.
  for (int state = state_names_accumulative[first_index];
      state < state_names_accumulative[first_index + 1]; ++state) {
    if (state_names[state].first_word_length != length)
      continue;

    bool state_match = false;
    size_t state_word = state_first_word;
    for (int pos = 0; true; ) {
      if (!WordLowerCaseEqualsASCII(words->at(state_word).begin,
          words->at(state_word).end, &state_names[state].string[pos]))
        break;

      pos += words->at(state_word).end - words->at(state_word).begin + 1;
      if (pos >= state_names[state].length) {
        state_match = true;
        break;
      }

      // Ran out of words, extract more from the tokenizer.
      if (++state_word == words->size()) {
        do {
          if (!tokenizer->GetNext())
            break;
        } while (tokenizer->token_is_delim());
        words->push_back(Word(tokenizer->token_begin(), tokenizer->token_end()));
      }
    }

    if (state_match) {
      *state_last_word = state_word;
      *state_index = state_names[state].state_index;
      return true;
    }
  }

  return false;
}

bool AddressDetector::IsZipValid(const Word& word, size_t state_index) {
  size_t length = word.end - word.begin;
  if (length != kZipDigits && length != kZipPlus4Digits + 1)
    return false;

  for (string16::const_iterator it = word.begin; it != word.end; ++it) {
    size_t pos = it - word.begin;
    if (IsAsciiDigit(*it) || (*it == '-' && pos == kZipDigits))
      continue;
    return false;
  }
  return IsZipValidForState(word, state_index);
}

bool AddressDetector::IsZipValidForState(const Word& word, size_t state_index)
{
    enum USState {
        AP = -4, // AP (military base in the Pacific)
        AA = -3, // AA (military base inside the US)
        AE = -2, // AE (military base outside the US)
        XX = -1, // (not in use)
        AK =  0, // AK Alaska
        AL =  1, // AL Alabama
        AR =  2, // AR Arkansas
        AS =  3, // AS American Samoa
        AZ =  4, // AZ Arizona
        CA =  5, // CA California
        CO =  6, // CO Colorado
        CT =  7, // CT Connecticut
        DC =  8, // DC District of Columbia
        DE =  9, // DE Delaware
        FL = 10, // FL Florida
        FM = 11, // FM Federated States of Micronesia
        GA = 12, // GA Georgia
        GU = 13, // GU Guam
        HI = 14, // HI Hawaii
        IA = 15, // IA Iowa
        ID = 16, // ID Idaho
        IL = 17, // IL Illinois
        IN = 18, // IN Indiana
        KS = 19, // KS Kansas
        KY = 20, // KY Kentucky
        LA = 21, // LA Louisiana
        MA = 22, // MA Massachusetts
        MD = 23, // MD Maryland
        ME = 24, // ME Maine
        MH = 25, // MH Marshall Islands
        MI = 26, // MI Michigan
        MN = 27, // MN Minnesota
        MO = 28, // MO Missouri
        MP = 29, // MP Northern Mariana Islands
        MS = 30, // MS Mississippi
        MT = 31, // MT Montana
        NC = 32, // NC North Carolina
        ND = 33, // ND North Dakota
        NE = 34, // NE Nebraska
        NH = 35, // NH New Hampshire
        NJ = 36, // NJ New Jersey
        NM = 37, // NM New Mexico
        NV = 38, // NV Nevada
        NY = 39, // NY New York
        OH = 40, // OH Ohio
        OK = 41, // OK Oklahoma
        OR = 42, // OR Oregon
        PA = 43, // PA Pennsylvania
        PR = 44, // PR Puerto Rico
        PW = 45, // PW Palau
        RI = 46, // RI Rhode Island
        SC = 47, // SC South Carolina
        SD = 48, // SD South Dakota
        TN = 49, // TN Tennessee
        TX = 50, // TX Texas
        UT = 51, // UT Utah
        VA = 52, // VA Virginia
        VI = 53, // VI Virgin Islands
        VT = 54, // VT Vermont
        WA = 55, // WA Washington
        WI = 56, // WI Wisconsin
        WV = 57, // WV West Virginia
        WY = 58, // WY Wyoming
    };

    static const USState stateForZipPrefix[] = {
    //   0   1   2   3   4   5   6   7   8   9
        XX, XX, XX, XX, XX, NY, PR, PR, VI, PR, // 000-009
        MA, MA, MA, MA, MA, MA, MA, MA, MA, MA, // 010-019
        MA, MA, MA, MA, MA, MA, MA, MA, RI, RI, // 020-029
        NH, NH, NH, NH, NH, NH, NH, NH, NH, ME, // 030-039
        ME, ME, ME, ME, ME, ME, ME, ME, ME, ME, // 040-049
        VT, VT, VT, VT, VT, MA, VT, VT, VT, VT, // 050-059
        CT, CT, CT, CT, CT, CT, CT, CT, CT, CT, // 060-069
        NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, // 070-079
        NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, NJ, // 080-089
        AE, AE, AE, AE, AE, AE, AE, AE, AE, XX, // 090-099
        NY, NY, NY, NY, NY, NY, NY, NY, NY, NY, // 100-109
        NY, NY, NY, NY, NY, NY, NY, NY, NY, NY, // 110-119
        NY, NY, NY, NY, NY, NY, NY, NY, NY, NY, // 120-129
        NY, NY, NY, NY, NY, NY, NY, NY, NY, NY, // 130-139
        NY, NY, NY, NY, NY, NY, NY, NY, NY, NY, // 140-149
        PA, PA, PA, PA, PA, PA, PA, PA, PA, PA, // 150-159
        PA, PA, PA, PA, PA, PA, PA, PA, PA, PA, // 160-169
        PA, PA, PA, PA, PA, PA, PA, PA, PA, PA, // 170-179
        PA, PA, PA, PA, PA, PA, PA, PA, PA, PA, // 180-189
        PA, PA, PA, PA, PA, PA, PA, DE, DE, DE, // 190-199
        DC, VA, DC, DC, DC, DC, MD, MD, MD, MD, // 200-209
        MD, MD, MD, XX, MD, MD, MD, MD, MD, MD, // 210-219
        VA, VA, VA, VA, VA, VA, VA, VA, VA, VA, // 220-229
        VA, VA, VA, VA, VA, VA, VA, VA, VA, VA, // 230-239
        VA, VA, VA, VA, VA, VA, VA, WV, WV, WV, // 240-249
        WV, WV, WV, WV, WV, WV, WV, WV, WV, WV, // 250-259
        WV, WV, WV, WV, WV, WV, WV, WV, WV, XX, // 260-269
        NC, NC, NC, NC, NC, NC, NC, NC, NC, NC, // 270-279
        NC, NC, NC, NC, NC, NC, NC, NC, NC, NC, // 280-289
        SC, SC, SC, SC, SC, SC, SC, SC, SC, SC, // 290-299
        GA, GA, GA, GA, GA, GA, GA, GA, GA, GA, // 300-309
        GA, GA, GA, GA, GA, GA, GA, GA, GA, GA, // 310-319
        FL, FL, FL, FL, FL, FL, FL, FL, FL, FL, // 320-329
        FL, FL, FL, FL, FL, FL, FL, FL, FL, FL, // 330-339
        AA, FL, FL, XX, FL, XX, FL, FL, XX, FL, // 340-349
        AL, AL, AL, XX, AL, AL, AL, AL, AL, AL, // 350-359
        AL, AL, AL, AL, AL, AL, AL, AL, AL, AL, // 360-369
        TN, TN, TN, TN, TN, TN, TN, TN, TN, TN, // 370-379
        TN, TN, TN, TN, TN, TN, MS, MS, MS, MS, // 380-389
        MS, MS, MS, MS, MS, MS, MS, MS, GA, GA, // 390-399
        KY, KY, KY, KY, KY, KY, KY, KY, KY, KY, // 400-409
        KY, KY, KY, KY, KY, KY, KY, KY, KY, XX, // 410-419
        KY, KY, KY, KY, KY, KY, KY, KY, XX, XX, // 420-429
        OH, OH, OH, OH, OH, OH, OH, OH, OH, OH, // 430-439
        OH, OH, OH, OH, OH, OH, OH, OH, OH, OH, // 440-449
        OH, OH, OH, OH, OH, OH, OH, OH, OH, OH, // 450-459
        IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, // 460-469
        IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, // 470-479
        MI, MI, MI, MI, MI, MI, MI, MI, MI, MI, // 480-489
        MI, MI, MI, MI, MI, MI, MI, MI, MI, MI, // 490-499
        IA, IA, IA, IA, IA, IA, IA, IA, IA, IA, // 500-509
        IA, IA, IA, IA, IA, IA, IA, XX, XX, XX, // 510-519
        IA, IA, IA, IA, IA, IA, IA, IA, IA, XX, // 520-529
        WI, WI, WI, XX, WI, WI, XX, WI, WI, WI, // 530-539
        WI, WI, WI, WI, WI, WI, WI, WI, WI, WI, // 540-549
        MN, MN, XX, MN, MN, MN, MN, MN, MN, MN, // 550-559
        MN, MN, MN, MN, MN, MN, MN, MN, XX, DC, // 560-569
        SD, SD, SD, SD, SD, SD, SD, SD, XX, XX, // 570-579
        ND, ND, ND, ND, ND, ND, ND, ND, ND, XX, // 580-589
        MT, MT, MT, MT, MT, MT, MT, MT, MT, MT, // 590-599
        IL, IL, IL, IL, IL, IL, IL, IL, IL, IL, // 600-609
        IL, IL, IL, IL, IL, IL, IL, IL, IL, IL, // 610-619
        IL, XX, IL, IL, IL, IL, IL, IL, IL, IL, // 620-629
        MO, MO, XX, MO, MO, MO, MO, MO, MO, MO, // 630-639
        MO, MO, XX, XX, MO, MO, MO, MO, MO, MO, // 640-649
        MO, MO, MO, MO, MO, MO, MO, MO, MO, XX, // 650-659
        KS, KS, KS, XX, KS, KS, KS, KS, KS, KS, // 660-669
        KS, KS, KS, KS, KS, KS, KS, KS, KS, KS, // 670-679
        NE, NE, XX, NE, NE, NE, NE, NE, NE, NE, // 680-689
        NE, NE, NE, NE, XX, XX, XX, XX, XX, XX, // 690-699
        LA, LA, XX, LA, LA, LA, LA, LA, LA, XX, // 700-709
        LA, LA, LA, LA, LA, XX, AR, AR, AR, AR, // 710-719
        AR, AR, AR, AR, AR, AR, AR, AR, AR, AR, // 720-729
        OK, OK, XX, TX, OK, OK, OK, OK, OK, OK, // 730-739
        OK, OK, XX, OK, OK, OK, OK, OK, OK, OK, // 740-749
        TX, TX, TX, TX, TX, TX, TX, TX, TX, TX, // 750-759
        TX, TX, TX, TX, TX, TX, TX, TX, TX, TX, // 760-769
        TX, XX, TX, TX, TX, TX, TX, TX, TX, TX, // 770-779
        TX, TX, TX, TX, TX, TX, TX, TX, TX, TX, // 780-789
        TX, TX, TX, TX, TX, TX, TX, TX, TX, TX, // 790-799
        CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, // 800-809
        CO, CO, CO, CO, CO, CO, CO, XX, XX, XX, // 810-819
        WY, WY, WY, WY, WY, WY, WY, WY, WY, WY, // 820-829
        WY, WY, ID, ID, ID, ID, ID, ID, ID, XX, // 830-839
        UT, UT, UT, UT, UT, UT, UT, UT, XX, XX, // 840-849
        AZ, AZ, AZ, AZ, XX, AZ, AZ, AZ, XX, AZ, // 850-859
        AZ, XX, XX, AZ, AZ, AZ, XX, XX, XX, XX, // 860-869
        NM, NM, NM, NM, NM, NM, XX, NM, NM, NM, // 870-879
        NM, NM, NM, NM, NM, TX, XX, XX, XX, NV, // 880-889
        NV, NV, XX, NV, NV, NV, XX, NV, NV, XX, // 890-899
        CA, CA, CA, CA, CA, CA, CA, CA, CA, XX, // 900-909
        CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, // 910-919
        CA, CA, CA, CA, CA, CA, CA, CA, CA, XX, // 920-929
        CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, // 930-939
        CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, // 940-949
        CA, CA, CA, CA, CA, CA, CA, CA, CA, CA, // 950-959
        CA, CA, AP, AP, AP, AP, AP, HI, HI, GU, // 960-969
        OR, OR, OR, OR, OR, OR, OR, OR, OR, OR, // 970-979
        WA, WA, WA, WA, WA, WA, WA, XX, WA, WA, // 980-989
        WA, WA, WA, WA, WA, AK, AK, AK, AK, AK, // 990-999
    };

    if (!word.begin || !word.end || (word.end - word.begin) < 3)
        return false;
    const char16* zipPtr = word.begin;
    if (zipPtr[0] < '0' || zipPtr[0] > '9' ||
        zipPtr[1] < '0' || zipPtr[1] > '9' ||
        zipPtr[2] < '0' || zipPtr[2] > '9')
        return false;

    int zip = zipPtr[0] - '0';
    zip *= 10;
    zip += zipPtr[1] - '0';
    zip *= 10;
    zip += zipPtr[2] - '0';
    return stateForZipPrefix[zip] == (int) state_index;
}

static const char* s_rawStreetSuffixes[] = {
    "allee", "alley", "ally", "aly",
    "anex", "annex", "anx", "arc", "arcade", "av", "ave", "aven", "avenu",
    "avenue", "avn", "avnue", "bayoo", "bayou", "bch", "beach", "bend",
    "bg", "bgs", "blf", "blfs", "bluf", "bluff", "bluffs", "blvd", "bnd",
    "bot", "bottm", "bottom", "boul", "boulevard", "boulv", "br", "branch",
    "brdge", "brg", "bridge", "brk", "brks", "brnch", "brook", "brooks",
    "btm", "burg", "burgs", "byp", "bypa", "bypas", "bypass", "byps", "byu",
    "camp", "canyn", "canyon", "cape", "causeway", "causway", "cen", "cent",
    "center", "centers", "centr", "centre", "cir", "circ", "circl",
    "circle", "circles", "cirs", "ck", "clb", "clf", "clfs", "cliff",
    "cliffs", "club", "cmn", "cmp", "cnter", "cntr", "cnyn", "common",
    "cor", "corner", "corners", "cors", "course", "court", "courts", "cove",
    "coves", "cp", "cpe", "cr", "crcl", "crcle", "crecent", "creek", "cres",
    "crescent", "cresent", "crest", "crk", "crossing", "crossroad",
    "crscnt", "crse", "crsent", "crsnt", "crssing", "crssng", "crst", "crt",
    "cswy", "ct", "ctr", "ctrs", "cts", "curv", "curve", "cv", "cvs", "cyn",
    "dale", "dam", "div", "divide", "dl", "dm", "dr", "driv", "drive",
    "drives", "drs", "drv", "dv", "dvd", "est", "estate", "estates", "ests",
    "exp", "expr", "express", "expressway", "expw", "expy", "ext",
    "extension", "extensions", "extn", "extnsn", "exts", "fall", "falls",
    "ferry", "field", "fields", "flat", "flats", "fld", "flds", "fls",
    "flt", "flts", "ford", "fords", "forest", "forests", "forg", "forge",
    "forges", "fork", "forks", "fort", "frd", "frds", "freeway", "freewy",
    "frg", "frgs", "frk", "frks", "frry", "frst", "frt", "frway", "frwy",
    "fry", "ft", "fwy", "garden", "gardens", "gardn", "gateway", "gatewy",
    "gatway", "gdn", "gdns", "glen", "glens", "gln", "glns", "grden",
    "grdn", "grdns", "green", "greens", "grn", "grns", "grov", "grove",
    "groves", "grv", "grvs", "gtway", "gtwy", "harb", "harbor", "harbors",
    "harbr", "haven", "havn", "hbr", "hbrs", "height", "heights", "hgts",
    "highway", "highwy", "hill", "hills", "hiway", "hiwy", "hl", "hllw",
    "hls", "hollow", "hollows", "holw", "holws", "hrbor", "ht", "hts",
    "hvn", "hway", "hwy", "inlet", "inlt", "is", "island", "islands",
    "isle", "isles", "islnd", "islnds", "iss", "jct", "jction", "jctn",
    "jctns", "jcts", "junction", "junctions", "junctn", "juncton", "key",
    "keys", "knl", "knls", "knol", "knoll", "knolls", "ky", "kys", "la",
    "lake", "lakes", "land", "landing", "lane", "lanes", "lck", "lcks",
    "ldg", "ldge", "lf", "lgt", "lgts", "light", "lights", "lk", "lks",
    "ln", "lndg", "lndng", "loaf", "lock", "locks", "lodg", "lodge", "loop",
    "loops", "mall", "manor", "manors", "mdw", "mdws", "meadow", "meadows",
    "medows", "mews", "mill", "mills", "mission", "missn", "ml", "mls",
    "mnr", "mnrs", "mnt", "mntain", "mntn", "mntns", "motorway", "mount",
    "mountain", "mountains", "mountin", "msn", "mssn", "mt", "mtin", "mtn",
    "mtns", "mtwy", "nck", "neck", "opas", "orch", "orchard", "orchrd",
    "oval", "overpass", "ovl", "park", "parks", "parkway", "parkways",
    "parkwy", "pass", "passage", "path", "paths", "pike", "pikes", "pine",
    "pines", "pk", "pkway", "pkwy", "pkwys", "pky", "pl", "place", "plain",
    "plaines", "plains", "plaza", "pln", "plns", "plz", "plza", "pne",
    "pnes", "point", "points", "port", "ports", "pr", "prairie", "prarie",
    "prk", "prr", "prt", "prts", "psge", "pt", "pts", "rad", "radial",
    "radiel", "radl", "ramp", "ranch", "ranches", "rapid", "rapids", "rd",
    "rdg", "rdge", "rdgs", "rds", "real", "rest", "ridge", "ridges", "riv", "river",
    "rivr", "rnch", "rnchs", "road", "roads", "route", "row", "rpd", "rpds",
    "rst", "rte", "rue", "run", "rvr", "shl", "shls", "shoal", "shoals",
    "shoar", "shoars", "shore", "shores", "shr", "shrs", "skwy", "skyway",
    "smt", "spg", "spgs", "spng", "spngs", "spring", "springs", "sprng",
    "sprngs", "spur", "spurs", "sq", "sqr", "sqre", "sqrs", "sqs", "squ",
    "square", "squares", "st", "sta", "station", "statn", "stn", "str",
    "stra", "strav", "strave", "straven", "stravenue", "stravn", "stream",
    "street", "streets", "streme", "strm", "strt", "strvn", "strvnue",
    "sts", "sumit", "sumitt", "summit", "ter", "terr", "terrace",
    "throughway", "tpk", "tpke", "tr", "trace", "traces", "track", "tracks",
    "trafficway", "trail", "trails", "trak", "trce", "trfy", "trk", "trks",
    "trl", "trls", "trnpk", "trpk", "trwy", "tunel", "tunl", "tunls",
    "tunnel", "tunnels", "tunnl", "turnpike", "turnpk", "un", "underpass",
    "union", "unions", "uns", "upas", "valley", "valleys", "vally", "vdct",
    "via", "viadct", "viaduct", "view", "views", "vill", "villag",
    "village", "villages", "ville", "villg", "villiage", "vis", "vist",
    "vista", "vl", "vlg", "vlgs", "vlly", "vly", "vlys", "vst", "vsta",
    "vw", "vws", "walk", "walks", "wall", "way", "ways", "well", "wells",
    "wl", "wls", "wy", "xing", "xrd",
    0,
};

bool AddressDetector::IsValidLocationName(const Word& word) {
    using namespace WTF;
    static HashSet<String> streetNames;
    if (!streetNames.size()) {
        const char** suffixes = s_rawStreetSuffixes;
        while (const char* suffix = *suffixes) {
            int index = suffix[0] - 'a';
            streetNames.add(suffix);
            suffixes++;
        }
    }
    char16 first_letter = base::ToLowerASCII(*word.begin);
    if (first_letter > 'z' || first_letter < 'a')
        return false;
    int index = first_letter - 'a';
    int length = std::distance(word.begin, word.end);
    if (*word.end == '.')
        length--;
    String value(word.begin, length);
    return streetNames.contains(value.lower());
}
