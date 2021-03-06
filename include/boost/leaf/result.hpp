#ifndef LEAF_2CD8E6B8CA8D11E8BD3B80D66CE5B91B
#define LEAF_2CD8E6B8CA8D11E8BD3B80D66CE5B91B

// Copyright (c) 2018-2019 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/exception.hpp>
#include <memory>
#include <climits>

namespace boost { namespace leaf {

	class bad_result:
		public std::exception,
		public error_id
	{
		char const * what() const noexcept final override
		{
			return "boost::leaf::bad_result";
		}

	public:

		explicit bad_result( error_id id ) noexcept:
			error_id(id)
		{
			assert(value());
		}
	};

	////////////////////////////////////////

	namespace leaf_detail
	{
		class result_discriminant
		{
			unsigned state_;

		public:

			enum kind_t
			{
				no_error = 0,
				err_id = 1,
				ctx_ptr = 2,
				val = 3
			};

			LEAF_CONSTEXPR explicit result_discriminant( error_id id ) noexcept:
				state_(id.value())
			{
				assert(state_==0 || (state_&3)==1);
			}

			struct kind_val { };
			LEAF_CONSTEXPR explicit result_discriminant( kind_val ) noexcept:
				state_(val)
			{
			}

			struct kind_ctx_ptr { };
			LEAF_CONSTEXPR explicit result_discriminant( kind_ctx_ptr ) noexcept:
				state_(ctx_ptr)
			{
			}

			LEAF_CONSTEXPR kind_t kind() const noexcept
			{
				return kind_t(state_&3);
			}

			LEAF_CONSTEXPR error_id get_error_id() const noexcept
			{
				assert(kind()==no_error || kind()==err_id);
				return leaf_detail::make_error_id(state_);
			}
		};
	}

	////////////////////////////////////////

	template <class T>
	class result
	{
		template <class U>
		friend class result;

		struct error_result
		{
			error_result( error_result && ) = default;
			error_result( error_result const & ) = delete;
			error_result & operator=( error_result const & ) = delete;

			LEAF_CONSTEXPR error_result( result & r ) noexcept:
				r_(r)
			{
			}

			result & r_;

			template <class U>
			LEAF_CONSTEXPR operator result<U>() noexcept
			{
				using leaf_detail::result_discriminant;
				switch(r_.what_.kind())
				{
				case result_discriminant::val:
					return result<U>(error_id());
				case result_discriminant::ctx_ptr:
					return result<U>(std::move(r_.ctx_));
				default:
					return result<U>(std::move(r_.what_));
				}
			}

			LEAF_CONSTEXPR operator error_id() noexcept
			{
				using leaf_detail::result_discriminant;
				switch(r_.what_.kind())
				{
				case result_discriminant::val:
					return error_id();
				case result_discriminant::ctx_ptr:
					return r_.ctx_->propagate_captured_errors();
				default:
					return r_.what_.get_error_id();
				}
			}
		};

		union
		{
			T value_;
			context_ptr ctx_;
		};

		leaf_detail::result_discriminant what_;

		LEAF_CONSTEXPR void destroy() const noexcept
		{
			using leaf_detail::result_discriminant;
			switch(this->what_.kind())
			{
			case result_discriminant::val:
				value_.~T();
				break;
			case result_discriminant::ctx_ptr:
				assert(!ctx_ || ctx_->captured_id_);
				ctx_.~context_ptr();
			default:
				break;
			}
		}

		template <class U>
		LEAF_CONSTEXPR leaf_detail::result_discriminant move_from( result<U> && x ) noexcept
		{
			using leaf_detail::result_discriminant;
			auto x_what = x.what_;
			switch(x_what.kind())
			{
			case result_discriminant::val:
				(void) new(&value_) T(std::move(x.value_));
				break;
			case result_discriminant::ctx_ptr:
				assert(!x.ctx_ || x.ctx_->captured_id_);
				(void) new(&ctx_) context_ptr(std::move(x.ctx_));
			default:
				break;
			}
			return x_what;
		}

		LEAF_CONSTEXPR result( leaf_detail::result_discriminant && what ) noexcept:
			what_(std::move(what))
		{
			using leaf_detail::result_discriminant;
			assert(what_.kind()==result_discriminant::err_id || what_.kind()==result_discriminant::no_error);
		}

	public:

		typedef T value_type;

		~result() noexcept
		{
			destroy();
		}

		LEAF_CONSTEXPR result( result && x ) noexcept:
			what_(move_from(std::move(x)))
		{
		}

		template <class U>
		LEAF_CONSTEXPR result( result<U> && x ) noexcept:
			what_(move_from(std::move(x)))

		{
		}

		LEAF_CONSTEXPR result():
			value_(T()),
			what_(leaf_detail::result_discriminant::kind_val{})
		{
		}

		LEAF_CONSTEXPR result( T && v ) noexcept:
			value_(std::move(v)),
			what_(leaf_detail::result_discriminant::kind_val{})
		{
		}

		LEAF_CONSTEXPR result( T const & v ):
			value_(v),
			what_(leaf_detail::result_discriminant::kind_val{})
		{
		}

		LEAF_CONSTEXPR result( error_id err ) noexcept:
			what_(err)
		{
		}

		LEAF_CONSTEXPR result( std::error_code const & ec ) noexcept:
			what_(error_id(ec))
		{
		}

		LEAF_CONSTEXPR result( context_ptr && ctx ) noexcept:
			ctx_(std::move(ctx)),
			what_(leaf_detail::result_discriminant::kind_ctx_ptr{})
		{
		}

		LEAF_CONSTEXPR result & operator=( result && x ) noexcept
		{
			destroy();
			what_ = move_from(std::move(x));
			return *this;
		}

		template <class U>
		LEAF_CONSTEXPR result & operator=( result<U> && x ) noexcept
		{
			destroy();
			what_ = move_from(std::move(x));
			return *this;
		}

		LEAF_CONSTEXPR explicit operator bool() const noexcept
		{
			using leaf_detail::result_discriminant;
			return what_.kind() == result_discriminant::val;
		}

		LEAF_CONSTEXPR T const & value() const
		{
			if( what_.kind() == leaf_detail::result_discriminant::val )
				return value_;
			else
				::boost::leaf::throw_exception(bad_result(get_error_id()));
		}

		LEAF_CONSTEXPR T & value()
		{
			if( what_.kind() == leaf_detail::result_discriminant::val )
				return value_;
			else
				::boost::leaf::throw_exception(bad_result(get_error_id()));
		}

		LEAF_CONSTEXPR T const & operator*() const
		{
			return value();
		}

		LEAF_CONSTEXPR T & operator*()
		{
			return value();
		}

		LEAF_CONSTEXPR T const * operator->() const
		{
			return &value();
		}

		LEAF_CONSTEXPR T * operator->()
		{
			return &value();
		}

		LEAF_CONSTEXPR error_id get_error_id() const noexcept
		{
			using leaf_detail::result_discriminant;
			assert(what_.kind()!=result_discriminant::val);
			return what_.kind()==result_discriminant::ctx_ptr ? ctx_->captured_id_ : what_.get_error_id();
		}

		LEAF_CONSTEXPR error_result error() noexcept
		{
			return error_result{*this};
		}

		template <class... E>
		LEAF_CONSTEXPR error_id load( E && ... e ) noexcept
		{
			return error_id(error()).load(std::forward<E>(e)...);
		}

		template <class... F>
		LEAF_CONSTEXPR error_id accumulate( F && ... f ) noexcept
		{
			return error_id(error()).accumulate(std::forward<F>(f)...);
		}
	};

	////////////////////////////////////////

	template <>
	class result<void>:
		result<bool>
	{
		typedef result<bool> base;

		template <class U>
		friend class result;

		LEAF_CONSTEXPR result( result<bool> && rb ):
			base(std::move(rb))
		{
		}

		LEAF_CONSTEXPR result( leaf_detail::result_discriminant && what ) noexcept:
			base(std::move(what))
		{
		}

	public:

		typedef void value_type;

		~result() noexcept
		{
		}

		LEAF_CONSTEXPR result( result && x ) noexcept:
			base(std::move(x))
		{
		}

		LEAF_CONSTEXPR result() noexcept
		{
		}

		LEAF_CONSTEXPR result( error_id err ) noexcept:
			base(err)
		{
		}

		LEAF_CONSTEXPR result( std::error_code const & ec ) noexcept:
			base(ec)
		{
		}

		LEAF_CONSTEXPR result( context_ptr && ctx ) noexcept:
			base(std::move(ctx))
		{
		}

		LEAF_CONSTEXPR void value() const
		{
			(void) base::value();
		}

		LEAF_CONSTEXPR void operator*() const
		{
			return value();
		}


		LEAF_CONSTEXPR void operator->() const
		{
			return value();
		}

		using base::operator=;
		using base::operator bool;
		using base::get_error_id;
		using base::error;
		using base::load;
		using base::accumulate;
	};

	////////////////////////////////////////

	template <class R>
	struct is_result_type;

	template <class T>
	struct is_result_type<result<T>>: std::true_type
	{
	};
} }

#endif
