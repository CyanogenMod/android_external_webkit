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

#ifndef CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#pragma once

#include "build/build_config.h"  // Needed for OS_ANDROID

#if defined(OS_ANDROID)

#include <vector>

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "content/content_detector.h"

// Finds a geographical address (currently US only) in the given text string.
class AddressDetector : public ContentDetector {
 public:
  AddressDetector();
  virtual ~AddressDetector();

  // Implementation of ContentDetector.
  virtual bool FindContent(const string16::const_iterator& begin,
                           const string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos) OVERRIDE;

 private:
  friend class AddressDetectorTest;

  virtual std::string GetContentText(const WebKit::WebRange& range) OVERRIDE;
  virtual GURL GetIntentURL(const std::string& content_text) OVERRIDE;
  virtual size_t GetMaximumContentLength() OVERRIDE;
  virtual bool IsEnabled(const WebKit::WebHitTestInfo& hit_test) OVERRIDE;

  // Internal structs and classes. Required to be visible by the unit tests.
  struct Word {
    string16::const_iterator begin;
    string16::const_iterator end;

    Word() {}
    Word(const string16::const_iterator& begin_it,
         const string16::const_iterator& end_it)
        : begin(begin_it),
          end(end_it) {
      DCHECK(begin_it <= end_it);
    }
  };

  class HouseNumberParser {
   public:
    HouseNumberParser() {}

    bool Parse(const string16::const_iterator& begin,
               const string16::const_iterator& end,
               Word* word);

   private:
    static inline bool IsPreDelimiter(char16 character);
    static inline bool IsPostDelimiter(char16 character);
    inline void RestartOnNextDelimiter();

    inline bool CheckFinished(Word* word) const;
    inline void AcceptChars(size_t num_chars);
    inline void SkipChars(size_t num_chars);
    inline void ResetState();

    // Iterators to the beginning, current position and ending of the string
    // being currently parsed.
    string16::const_iterator begin_;
    string16::const_iterator it_;
    string16::const_iterator end_;

    // Number of digits found in the current result candidate.
    size_t num_digits_;

    // Number of characters previous to the current iterator that belong
    // to the current result candidate.
    size_t result_chars_;

    DISALLOW_COPY_AND_ASSIGN(HouseNumberParser);
  };

  typedef std::vector<Word> WordList;
  typedef StringTokenizerT<string16, string16::const_iterator>
      String16Tokenizer;

  static bool FindStateStartingInWord(WordList* words,
                                      size_t state_first_word,
                                      size_t* state_last_word,
                                      String16Tokenizer* tokenizer,
                                      size_t* state_index);

  static bool IsValidLocationName(const Word& word);
  static bool IsZipValid(const Word& word, size_t state_index);
  static bool IsZipValidForState(const Word& word, size_t state_index);

  DISALLOW_COPY_AND_ASSIGN(AddressDetector);
};

#endif  // defined(OS_ANDROID)

#endif  // CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
