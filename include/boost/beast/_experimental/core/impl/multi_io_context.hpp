//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_MULTI_IO_CONTEXT_HPP
#define BOOST_BEAST_IMPL_MULTI_IO_CONTEXT_HPP

#include <boost/beast/_experimental/core/multi_io_context.hpp>

namespace boost {
namespace beast {

struct multi_io_context::element
{
    element* prev;
    element* next;

    std::atomic<std::size_t> count;
    net::io_context ioc;
    net::executor_work_guard<
        net::io_context::executor_type> wg;

    element()
        : count(0)
        , ioc(concurrency_hint_)
        , wg(ioc.get_executor())
    {
    }
};

//----------------------------------------------------------

multi_io_context::
table::
~table()
{
    delete[] v_;
}

multi_io_context::
table::
table(std::size_t n)
    : n_(n)
    , v_(new element[n])
{
}

auto
multi_io_context::
table::
begin() noexcept ->
    element*
{
    return &v_[0];
}

auto
multi_io_context::
table::
end() noexcept ->
    element*
{
    return &v_[n_];
}

std::size_t
multi_io_context::
table::
size() const noexcept
{
    return n_;
}

//----------------------------------------------------------

multi_io_context::
executor_type::
executor_type(
    element& e)
    : e_(&e)
{
    ++e_->count;
}

multi_io_context::
executor_type::
~executor_type()
{
    --e_->count;
}

multi_io_context::
executor_type::
executor_type(
    executor_type const& ex) noexcept
    : e_(ex.e_)
{
    ++e_->count;
}

net::io_context&
multi_io_context::
executor_type::
context() const noexcept
{
    return e_->ioc;
}

auto
multi_io_context::
executor_type::
operator=(
    executor_type const& ex) noexcept ->
        executor_type&
{
    ++ex.e_->count;
    --e_->count;
    e_ = ex.e_;
    return *this;
}

void
multi_io_context::
executor_type::
on_work_started() const noexcept
{
    e_->ioc.get_executor().on_work_started();
}

void
multi_io_context::
executor_type::
on_work_finished() const noexcept
{
    e_->ioc.get_executor().on_work_finished();
}

template<class F, class Alloc>
void
multi_io_context::
executor_type::
dispatch(F&& f, Alloc const& a)
{
    e_->ioc.get_executor().dispatch(
        std::forward<F>(f), a);
}

template<class F, class Alloc>
void
multi_io_context::
executor_type::
post(F&& f, Alloc const& a)
{
    e_->ioc.get_executor().post(
        std::forward<F>(f), a);
}

template<class F, class Alloc>
void
multi_io_context::
executor_type::
defer(F&& f, Alloc const& a)
{
    e_->ioc.get_executor().defer(
        std::forward<F>(f), a);
}

//----------------------------------------------------------

multi_io_context::
multi_io_context(
    std::size_t number_of_threads)
    : tab_(number_of_threads)
    , idle_(nullptr)
{
    for(auto& e : tab_)
    {
        e.next = idle_;
        e.prev = nullptr;
        if(idle_)
            idle_->prev = &e;
        idle_ = &e;
    }
}

auto
multi_io_context::
make_executor() noexcept ->
    executor_type
{
    boost::shared_lock<
        boost::shared_mutex> lock(m_);
    element* best = nullptr;
    auto n = std::size_t(-1);
    for(auto& e : tab_)
    {
        auto count = e.count.load();
        if(count < n)
        {
            n = count;
            best = &e;
        }
    }
    return executor_type(*best);
}

void
multi_io_context::
run()
{
    struct cleanup
    {
        element* e;
        multi_io_context& ioc;

        ~cleanup()
        {
            // put the context back in idle list
            boost::unique_lock<
                boost::shared_mutex> lock(ioc.m_);
            e->prev = nullptr;
            e->next = ioc.idle_;
            if( ioc.idle_)
                ioc.idle_->prev = e;
            ioc.idle_ = e;
        }
    };

    element* e;

    {
        // get the next idle context
        boost::unique_lock<
            boost::shared_mutex> lock(m_);
        e = idle_;
        if(e)
        {
            idle_ = idle_->next;
            if(idle_)
                idle_->prev = nullptr;
        }
    }

    if(e)
    {
        // exception safety
        cleanup c{e, *this};

        e->ioc.run();
    }
}

void
multi_io_context::
stop()
{
    boost::unique_lock<
        boost::shared_mutex> lock(m_);
    for(auto& e : tab_)
        e.wg.reset();
}

} // beast
} // boost

#include <boost/beast/_experimental/core/impl/multi_io_context.hpp>

#endif
