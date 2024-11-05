#pragma once

#include <memory>
#include <coroutine>

namespace aifs {
    /**
     * Type-erased awaitable type for polymorphic interfaces with
     * awaitable operations.
     */
    template <typename T = void>
    class awaitable {
    private:
        struct awaitable_concept {
            virtual ~awaitable_concept() = default;
            virtual T await_resume() = 0;
            virtual void await_suspend(std::coroutine_handle<>) noexcept = 0;
        };

        template <typename Impl>
        struct model : awaitable_concept {
            model(Impl&& i) : impl_{std::move(i)}
            {}

            T await_resume() override {
                return impl_.await_resume();
            }

            void await_suspend(std::coroutine_handle<> h) noexcept override {
                impl_.await_suspend(h);
            }

            Impl impl_;
        };

    public:
        template <typename Impl>
        awaitable(Impl&& impl)
                : impl_{std::make_unique<model<Impl>>(std::forward<Impl>(impl))}
        {}

        [[nodiscard]] constexpr bool await_ready() const noexcept {
            return false;
        }

        [[nodiscard]] T await_resume() {
            return impl_->await_resume();
        }

        void await_suspend(std::coroutine_handle<> h) noexcept {
            impl_->await_suspend(h);
        }

    private:
        std::unique_ptr<awaitable_concept> impl_;
    };
}