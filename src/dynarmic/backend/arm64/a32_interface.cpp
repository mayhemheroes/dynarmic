/* This file is part of the dynarmic project.
 * Copyright (c) 2021 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <memory>
#include <mutex>

#include <boost/icl/interval_set.hpp>
#include <mcl/assert.hpp>
#include <mcl/scope_exit.hpp>
#include <mcl/stdint.hpp>

#include "dynarmic/backend/arm64/a32_address_space.h"
#include "dynarmic/backend/arm64/a32_core.h"
#include "dynarmic/backend/arm64/a32_jitstate.h"
#include "dynarmic/common/atomic.h"
#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/context.h"

namespace Dynarmic::A32 {

using namespace Backend::Arm64;

struct Context::Impl {
    A32JitState state;
};

Context::Context()
        : impl(std::make_unique<Context::Impl>()) {}

Context::~Context() = default;

Context::Context(const Context& ctx)
        : impl(std::make_unique<Context::Impl>(*ctx.impl)) {}

Context::Context(Context&& ctx) noexcept
        : impl(std::move(ctx.impl)) {}

Context& Context::operator=(const Context& ctx) {
    *impl = *ctx.impl;
    return *this;
}

Context& Context::operator=(Context&& ctx) noexcept {
    impl = std::move(ctx.impl);
    return *this;
}

std::array<std::uint32_t, 16>& Context::Regs() {
    return impl->state.regs;
}
const std::array<std::uint32_t, 16>& Context::Regs() const {
    return impl->state.regs;
}
std::array<std::uint32_t, 64>& Context::ExtRegs() {
    return impl->state.ext_regs;
}
const std::array<std::uint32_t, 64>& Context::ExtRegs() const {
    return impl->state.ext_regs;
}

std::uint32_t Context::Cpsr() const {
    return impl->state.Cpsr();
}
void Context::SetCpsr(std::uint32_t value) {
    impl->state.SetCpsr(value);
}

std::uint32_t Context::Fpscr() const {
    return impl->state.Fpscr();
}
void Context::SetFpscr(std::uint32_t value) {
    return impl->state.SetFpscr(value);
}

struct Jit::Impl final {
    Impl(Jit* jit_interface, A32::UserConfig conf)
            : jit_interface(jit_interface)
            , conf(conf)
            , current_address_space(conf)
            , core(conf) {}

    HaltReason Run() {
        ASSERT(!jit_interface->is_executing);
        PerformRequestedCacheInvalidation();

        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Run(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation();

        return hr;
    }

    HaltReason Step() {
        ASSERT(!jit_interface->is_executing);
        PerformRequestedCacheInvalidation();

        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Step(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation();

        return hr;
    }

    void ClearCache() {
        std::unique_lock lock{invalidation_mutex};
        invalidate_entire_cache = true;
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(std::uint32_t start_address, std::size_t length) {
        std::unique_lock lock{invalidation_mutex};
        invalid_cache_ranges.add(boost::icl::discrete_interval<u32>::closed(start_address, static_cast<u32>(start_address + length - 1)));
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        current_state = {};
    }

    void HaltExecution(HaltReason hr) {
        Atomic::Or(&halt_reason, static_cast<u32>(hr));
    }

    void ClearHalt(HaltReason hr) {
        Atomic::And(&halt_reason, ~static_cast<u32>(hr));
    }

    std::array<std::uint32_t, 16>& Regs() {
        return current_state.regs;
    }

    const std::array<std::uint32_t, 16>& Regs() const {
        return current_state.regs;
    }

    std::array<std::uint32_t, 64>& ExtRegs() {
        return current_state.ext_regs;
    }

    const std::array<std::uint32_t, 64>& ExtRegs() const {
        return current_state.ext_regs;
    }

    std::uint32_t Cpsr() const {
        return current_state.Cpsr();
    }

    void SetCpsr(std::uint32_t value) {
        current_state.SetCpsr(value);
    }

    std::uint32_t Fpscr() const {
        return current_state.Fpscr();
    }

    void SetFpscr(std::uint32_t value) {
        current_state.SetFpscr(value);
    }

    Context SaveContext() const {
        Context ctx;
        ctx.impl->state = current_state;
        return ctx;
    }

    void SaveContext(Context& ctx) const {
        ctx.impl->state = current_state;
    }

    void LoadContext(const Context& ctx) {
        current_state = ctx.impl->state;
    }

    void ClearExclusiveState() {
        current_state.exclusive_state = false;
    }

    void DumpDisassembly() const {
        ASSERT_FALSE("Unimplemented");
    }

private:
    void PerformRequestedCacheInvalidation() {
        ClearHalt(HaltReason::CacheInvalidation);

        if (invalidate_entire_cache) {
            current_address_space.ClearCache();

            invalidate_entire_cache = false;
            invalid_cache_ranges.clear();
            return;
        }

        if (!invalid_cache_ranges.empty()) {
            // TODO: Optimize
            current_address_space.ClearCache();

            invalid_cache_ranges.clear();
            return;
        }
    }

    Jit* jit_interface;
    A32::UserConfig conf;
    A32JitState current_state{};
    A32AddressSpace current_address_space;
    A32Core core;

    volatile u32 halt_reason = 0;

    std::mutex invalidation_mutex;
    boost::icl::interval_set<u32> invalid_cache_ranges;
    bool invalidate_entire_cache = false;
};

Jit::Jit(UserConfig conf)
        : impl(std::make_unique<Impl>(this, conf)) {}

Jit::~Jit() = default;

HaltReason Jit::Run() {
    return impl->Run();
}

HaltReason Jit::Step() {
    return impl->Step();
}

void Jit::ClearCache() {
    impl->ClearCache();
}

void Jit::InvalidateCacheRange(std::uint32_t start_address, std::size_t length) {
    impl->InvalidateCacheRange(start_address, length);
}

void Jit::Reset() {
    impl->Reset();
}

void Jit::HaltExecution(HaltReason hr) {
    impl->HaltExecution(hr);
}

void Jit::ClearHalt(HaltReason hr) {
    impl->ClearHalt(hr);
}

std::array<std::uint32_t, 16>& Jit::Regs() {
    return impl->Regs();
}

const std::array<std::uint32_t, 16>& Jit::Regs() const {
    return impl->Regs();
}

std::array<std::uint32_t, 64>& Jit::ExtRegs() {
    return impl->ExtRegs();
}

const std::array<std::uint32_t, 64>& Jit::ExtRegs() const {
    return impl->ExtRegs();
}

std::uint32_t Jit::Cpsr() const {
    return impl->Cpsr();
}

void Jit::SetCpsr(std::uint32_t value) {
    impl->SetCpsr(value);
}

std::uint32_t Jit::Fpscr() const {
    return impl->Fpscr();
}

void Jit::SetFpscr(std::uint32_t value) {
    impl->SetFpscr(value);
}

Context Jit::SaveContext() const {
    return impl->SaveContext();
}

void Jit::SaveContext(Context& ctx) const {
    impl->SaveContext(ctx);
}

void Jit::LoadContext(const Context& ctx) {
    impl->LoadContext(ctx);
}

void Jit::ClearExclusiveState() {
    impl->ClearExclusiveState();
}

void Jit::DumpDisassembly() const {
    impl->DumpDisassembly();
}

}  // namespace Dynarmic::A32
