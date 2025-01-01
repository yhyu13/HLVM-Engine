/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#define HLVM_INLINE_FUNC inline
#define HLVM_STATIC_FUNC static
#define HLVM_EXTERN_FUNC extern
#define HLVM_EXTERN_VAR extern
#define HLVM_INLINE_VAR inline
#define HLVM_STATIC_VAR static
#define HLVM_THREAD_LOCAL_VAR thread_local

#define HLVM_UNLIKELY [[unlikely]]
#define HLVM_LIKELY [[likely]]
#define HLVM_NORETURN [[noreturn]]
#define HLVM_NODISCARD [[nodiscard]]
#define HLVM_MAYBEUNUSED [[maybe_unused]]
