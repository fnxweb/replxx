/*
 * Copyright (c) 2017-2018, Marcin Konarski (amok at codestation.org)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HAVE_REPLXX_REPLXX_IMPL_HXX_INCLUDED
#define HAVE_REPLXX_REPLXX_IMPL_HXX_INCLUDED 1

#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "replxx.hxx"
#include "history.hxx"
#include "killring.hxx"
#include "utf8string.hxx"
#include "prompt.hxx"
#include "io.hxx"

namespace replxx {

class Replxx::ReplxxImpl {
public:
	class Completion {
		UnicodeString _text;
		Replxx::Color _color;
	public:
		Completion( UnicodeString const& text_, Replxx::Color color_ )
			: _text( text_ )
			, _color( color_ ) {
		}
		Completion( Replxx::Completion const& completion_ )
			: _text( completion_.text() )
			, _color( completion_.color() ) {
		}
		Completion( Completion const& ) = default;
		Completion& operator = ( Completion const& ) = default;
		Completion( Completion&& ) = default;
		Completion& operator = ( Completion&& ) = default;
		UnicodeString const& text( void ) const {
			return ( _text );
		}
		Replxx::Color color( void ) const {
			return ( _color );
		}
	};
	typedef std::vector<Completion> completions_t;
	typedef std::vector<UnicodeString> hints_t;
	typedef std::unique_ptr<char[]> utf8_buffer_t;
	typedef std::unique_ptr<char32_t[]> input_buffer_t;
	typedef std::vector<char> char_widths_t;
	typedef std::vector<char32_t> display_t;
	typedef std::deque<char32_t> key_presses_t;
	typedef std::deque<std::string> messages_t;
	enum class HINT_ACTION {
		REGENERATE,
		REPAINT,
		TRIM,
		SKIP
	};
	typedef Replxx::ACTION_RESULT ( ReplxxImpl::* key_press_handler_raw_t )( char32_t );
	typedef std::unordered_map<int, Replxx::key_press_handler_t> key_press_handlers_t;
private:
	typedef int long long unsigned action_trait_t;
	static action_trait_t const NOOP                   =  0;
	static action_trait_t const WANT_REFRESH           =  1;
	static action_trait_t const RESET_KILL_ACTION      =  2;
	static action_trait_t const SET_KILL_ACTION        =  4;
	static action_trait_t const DONT_RESET_PREFIX      =  8;
	static action_trait_t const DONT_RESET_COMPLETIONS = 16;
private:
	mutable Utf8String     _utf8Buffer;
	UnicodeString  _data;
	char_widths_t  _charWidths; // character widths from mk_wcwidth()
	display_t      _display;
	int _displayInputLength;
	UnicodeString  _hint;
	int _pos;    // character position in buffer ( 0 <= _pos <= _len )
	int _prefix; // prefix length used in common prefix search
	int _hintSelection; // Currently selected hint.
	History _history;
	KillRing _killRing;
	int _maxHintRows;
	int _hintDelay;
	char const* _breakChars;
	int _completionCountCutoff;
	bool _overwrite;
	bool _doubleTabCompletion;
	bool _completeOnEmpty;
	bool _beepOnAmbiguousCompletion;
	bool _immediateCompletion;
	bool _noColor;
	key_press_handlers_t _keyPressHandlers;
	Terminal _terminal;
	std::thread::id _currentThread;
	Prompt _prompt;
	Replxx::completion_callback_t _completionCallback;
	Replxx::highlighter_callback_t _highlighterCallback;
	Replxx::hint_callback_t _hintCallback;
	key_presses_t _keyPresses;
	messages_t _messages;
	completions_t _completions;
	int _completionContextLength;
	int _completionSelection;
	std::string _preloadedBuffer; // used with set_preload_buffer
	std::string _errorMessage;
	bool _modifiedState;
	mutable std::mutex _mutex;
public:
	ReplxxImpl( FILE*, FILE*, FILE* );
	void set_completion_callback( Replxx::completion_callback_t const& fn );
	void set_highlighter_callback( Replxx::highlighter_callback_t const& fn );
	void set_hint_callback( Replxx::hint_callback_t const& fn );
	char const* input( std::string const& prompt );
	void history_add( std::string const& line );
	int history_save( std::string const& filename );
	int history_load( std::string const& filename );
	std::string history_line( int index );
	int history_size( void ) const;
	void set_preload_buffer(std::string const& preloadText);
	void set_word_break_characters( char const* wordBreakers );
	void set_max_hint_rows( int count );
	void set_hint_delay( int milliseconds );
	void set_double_tab_completion( bool val );
	void set_complete_on_empty( bool val );
	void set_beep_on_ambiguous_completion( bool val );
	void set_immediate_completion( bool val );
	void set_unique_history( bool );
	void set_no_color( bool val );
	void set_max_history_size( int len );
	void set_completion_count_cutoff( int len );
	int install_window_change_handler( void );
	completions_t call_completer( std::string const& input, int& ) const;
	hints_t call_hinter( std::string const& input, int&, Replxx::Color& color ) const;
	void print( char const*, int );
	Replxx::ACTION_RESULT clear_screen( char32_t );
	void emulate_key_press( char32_t );
	Replxx::ACTION_RESULT invoke( Replxx::ACTION, char32_t );
	void bind_key( char32_t, Replxx::key_press_handler_t );
	Replxx::State get_state( void ) const;
	void set_state( Replxx::State const& );
private:
	ReplxxImpl( ReplxxImpl const& ) = delete;
	ReplxxImpl& operator = ( ReplxxImpl const& ) = delete;
private:
	void preload_puffer( char const* preloadText );
	int get_input_line( void );
	Replxx::ACTION_RESULT action( action_trait_t, key_press_handler_raw_t const&, char32_t );
	Replxx::ACTION_RESULT insert_character( char32_t );
	Replxx::ACTION_RESULT go_to_begining_of_line( char32_t );
	Replxx::ACTION_RESULT go_to_end_of_line( char32_t );
	Replxx::ACTION_RESULT move_one_char_left( char32_t );
	Replxx::ACTION_RESULT move_one_char_right( char32_t );
	Replxx::ACTION_RESULT move_one_word_left( char32_t );
	Replxx::ACTION_RESULT move_one_word_right( char32_t );
	Replxx::ACTION_RESULT kill_word_to_left( char32_t );
	Replxx::ACTION_RESULT kill_word_to_right( char32_t );
	Replxx::ACTION_RESULT kill_to_whitespace_to_left( char32_t );
	Replxx::ACTION_RESULT kill_to_begining_of_line( char32_t );
	Replxx::ACTION_RESULT kill_to_end_of_line( char32_t );
	Replxx::ACTION_RESULT yank( char32_t );
	Replxx::ACTION_RESULT yank_cycle( char32_t );
	Replxx::ACTION_RESULT capitalize_word( char32_t );
	Replxx::ACTION_RESULT lowercase_word( char32_t );
	Replxx::ACTION_RESULT uppercase_word( char32_t );
	Replxx::ACTION_RESULT transpose_characters( char32_t );
	Replxx::ACTION_RESULT abort_line( char32_t );
	Replxx::ACTION_RESULT send_eof( char32_t );
	Replxx::ACTION_RESULT delete_character( char32_t );
	Replxx::ACTION_RESULT backspace_character( char32_t );
	Replxx::ACTION_RESULT commit_line( char32_t );
	Replxx::ACTION_RESULT history_next( char32_t );
	Replxx::ACTION_RESULT history_previous( char32_t );
	Replxx::ACTION_RESULT history_move( bool );
	Replxx::ACTION_RESULT history_first( char32_t );
	Replxx::ACTION_RESULT history_last( char32_t );
	Replxx::ACTION_RESULT history_jump( bool );
	Replxx::ACTION_RESULT hint_next( char32_t );
	Replxx::ACTION_RESULT hint_previous( char32_t );
	Replxx::ACTION_RESULT hint_move( bool );
	Replxx::ACTION_RESULT toggle_overwrite_mode( char32_t );
#ifndef _WIN32
	Replxx::ACTION_RESULT verbatim_insert( char32_t );
	Replxx::ACTION_RESULT suspend( char32_t );
#endif
	Replxx::ACTION_RESULT complete_line( char32_t );
	Replxx::ACTION_RESULT complete_next( char32_t );
	Replxx::ACTION_RESULT complete_previous( char32_t );
	Replxx::ACTION_RESULT complete( bool );
	Replxx::ACTION_RESULT incremental_history_search( char32_t startChar );
	Replxx::ACTION_RESULT common_prefix_search( char32_t startChar );
	char32_t read_char( HINT_ACTION = HINT_ACTION::SKIP );
	char const* read_from_stdin( void );
	char32_t do_complete_line( bool );
	void refresh_line( HINT_ACTION = HINT_ACTION::REGENERATE );
	void render( char32_t );
	void render( HINT_ACTION );
	int handle_hints( HINT_ACTION );
	void set_color( Replxx::Color );
	int context_length( void );
	void clear( void );
	void repaint( void );
	bool is_word_break_character( char32_t ) const;
	void dynamicRefresh(Prompt& pi, char32_t* buf32, int len, int pos);
	char const* finalize_input( char const* );
	void clear_self_to_end_of_screen( void );
	typedef struct {
		int index;
		bool error;
	} paren_info_t;
	paren_info_t matching_paren( void );
};

}

#endif

