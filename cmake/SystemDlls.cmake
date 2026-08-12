# DLLs that ship with Windows and must never be copied into a deployment.
# Anything imported but NOT matching one of these has to travel with the exe.
#
# Shared by CheckDeps.cmake (which reports them) and DeployDeps.cmake (which
# copies the rest), so the two can never disagree about what counts as a system
# library.
#
# api-ms-win-crt-* is the UCRT, shipped with Windows since 10; msvcrt.dll has
# been present far longer.

set(VBIRD_SYSTEM_DLL_PATTERNS
    "^api-ms-win-"
    "^ext-ms-win-"
    "^(kernel32|kernelbase|user32|gdi32|gdi32full|shell32|shlwapi|ole32|oleaut32)\\.dll$"
    "^(advapi32|comdlg32|comctl32|version|winmm|imm32|uxtheme|dwmapi|msimg32)\\.dll$"
    "^(ws2_32|iphlpapi|dnsapi|mpr|netapi32|userenv|secur32|crypt32|bcrypt|ncrypt)\\.dll$"
    "^(setupapi|cfgmgr32|rpcrt4|ntdll|msvcrt|d3d11|d3d9|d3d12|dxgi|dxva2|opengl32|glu32)\\.dll$"
    "^(authz|wtsapi32|winspool\\.drv|oleacc|propsys|windowscodecs|mf|mfplat|mfreadwrite)\\.dll$"
    "^(dbghelp|psapi|powrprof|winhttp|wininet|urlmon|imagehlp|avicap32|msacm32)\\.dll$"
    "^(d3dcompiler_[0-9]+|dcomp|directxmath|xinput[0-9_]*|dsound|dinput8)\\.dll$"
    # Text and shell stacks Qt6Gui pulls in: DirectWrite, Direct2D, Uniscribe,
    # and the per-monitor DPI API.
    "^(dwrite|d2d1|usp10|shcore|combase|sechost|ucrtbase|win32u|wintrust)\\.dll$"
    "^(normaliz|wldap32|cryptbase|bcryptprimitives|profapi|winsta|dhcpcsvc)\\.dll$"
)

function(vbird_is_system_dll dll_name out_var)
    string(TOLOWER "${dll_name}" _lower)
    foreach(_pat IN LISTS VBIRD_SYSTEM_DLL_PATTERNS)
        if(_lower MATCHES "${_pat}")
            set(${out_var} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} FALSE PARENT_SCOPE)
endfunction()

# Extracts the imported DLL names from a PE file using objdump.
function(vbird_get_imports pe_file out_var)
    find_program(OBJDUMP NAMES objdump llvm-objdump)
    if(NOT OBJDUMP)
        message(FATAL_ERROR "objdump not found on PATH")
    endif()

    execute_process(
        COMMAND "${OBJDUMP}" -p "${pe_file}"
        OUTPUT_VARIABLE _dump
        ERROR_QUIET
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    set(_result "")
    string(REGEX MATCHALL "DLL Name: [^\r\n]+" _lines "${_dump}")
    foreach(_line IN LISTS _lines)
        string(REPLACE "DLL Name: " "" _dll "${_line}")
        string(STRIP "${_dll}" _dll)
        list(APPEND _result "${_dll}")
    endforeach()

    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()
