#pragma once

#include <coroutine>
#include <memory>

namespace aifs {
/**
 * Type-erased Awaitable type for polymorphic interfaces with
 * Awaitable operations.
 */
template <typename T = void>
class Awaitable {
private:
    struct Concept {
        virtual ~Concept() = default;
        virtual T await_resume() = 0;
        virtual void await_suspend(std::coroutine_handle<>) noexcept = 0;
    };

    template <typename Impl>
    struct Model : Concept {
        Model(Impl&& i)
            : impl_ { std::move(i) }
        {
        }

        T await_resume() override { return impl_.await_resume(); }

        void await_suspend(std::coroutine_handle<> h) noexcept override
        {
            impl_.await_suspend(h);
        }

        Impl impl_;
    };

public:
    template <typename Impl>
    Awaitable(Impl&& impl)
        : impl_ { std::make_unique<Model<Impl>>(std::forward<Impl>(impl)) }
    {
    }

    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

    [[nodiscard]] T await_resume() { return impl_->await_resume(); }

    void await_suspend(std::coroutine_handle<> h) noexcept
    {
        impl_->await_suspend(h);
    }

private:
    std::unique_ptr<Concept> impl_;
};
} // namespace aifs