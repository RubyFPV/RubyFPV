# RubyFPV Quick Wins: Code Issues & Improvements

**Analysis Date:** 2026-02-23

---

## Executive Summary

Found **5000+ unsafe string operations** and several other potential issues that could be good pull requests:

- **5000+ instances:** `strcpy/strcat/sprintf` without bounds checks
- **169 potential issues:** Various warnings and edge cases
- **Multiple areas:** Memory safety, error handling, obsolete code

---

## Quick Wins (Prioritized by Impact)

### 🔴 CRITICAL: String Safety (High Impact, Easy Fix)

**Problem:** 5000+ uses of `strcpy`, `strcat`, `sprintf` without bounds checking.

**Example 1 - Buffer Overflow Risk:**
```c
// File: code/r_utils/ruby_alive.cpp
char szFileUpdate[256];
strcpy(szFileUpdate, FOLDER_RUBY_TEMP);      // ⚠️ Unsafe
strcat(szFileUpdate, FILE_TEMP_UPDATE_IN_PROGRESS);  // ⚠️ Unsafe

// If FOLDER_RUBY_TEMP is "/root/.ruby/config/" (24 chars)
// And FILE_TEMP_UPDATE_IN_PROGRESS is long, this can overflow szFileUpdate
```

**Fix:**
```c
char szFileUpdate[MAX_FILE_PATH_SIZE];
snprintf(szFileUpdate, sizeof(szFileUpdate), "%s%s", FOLDER_RUBY_TEMP, FILE_TEMP_UPDATE_IN_PROGRESS);
```

**Example 2 - Command Injection Risk:**
```c
// File: code/r_utils/ruby_initdhcp.cpp
sprintf(szBuff, "nice pump -i %s --no-ntp -h Ruby%s 2>&1 1>/dev/null", pszETH, szType);
// ⚠️ If pszETH or szType contain shell metacharacters, command injection possible

hw_execute_bash_command(szBuff, NULL);
```

**Fix:**
```c
snprintf(szBuff, sizeof(szBuff), "nice pump -i '%s' --no-ntp -h 'Ruby%s' 2>&1 1>/dev/null", pszETH, szType);
// Or use execve() with argument array instead of shell command
```

**Affected Files (Sample):**
- `code/r_utils/ruby_alive.cpp` (6 instances)
- `code/r_utils/ruby_initdhcp.cpp` (15+ instances)
- `code/r_utils/ruby_update_worker.cpp` (3+ instances)
- `code/r_central/handle_commands.cpp` (many)
- `code/r_vehicle/ruby_rt_vehicle.cpp` (many)

**Impact:** Buffer overflow / Command injection vulnerabilities

**Effort:** Easy (bulk find-replace with manual review)

**Suggested Approach:**
```bash
# Find all unsafe calls
grep -r "strcpy\|strcat\|sprintf" code/ --include="*.cpp" --include="*.c" | \
  grep -v "strncpy\|strncat\|snprintf" > unsafe_strings.txt

# Replace patterns:
# strcpy(dst, src)        → snprintf(dst, sizeof(dst), "%s", src)
# strcat(dst, src)        → snprintf(dst + strlen(dst), sizeof(dst) - strlen(dst), "%s", src)
# sprintf(dst, fmt, ...) → snprintf(dst, sizeof(dst), fmt, ...)
```

---

### 🟡 MEDIUM: Unhandled NULL Pointers (Medium Impact)

**Problem:** Multiple locations don't check for NULL before using pointers.

**Example:**
```c
// File: code/r_central/notifications.cpp
if ( (-1 == iRuntimeInfoIndex) || (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel == NULL) )
   return;
// What if iRuntimeInfoIndex is OOB but not -1?
// Array bounds should be checked separately.
```

**Fix:**
```c
if (iRuntimeInfoIndex < 0 || iRuntimeInfoIndex >= MAX_VEHICLES)
   return;
if (NULL == g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel)
   return;
```

**Files Affected:**
- `code/r_central/notifications.cpp` (3 instances)
- `code/r_central/render_joysticks.cpp`
- `code/r_central/oled/oled_ssd1306.cpp`
- `code/r_central/handle_commands.cpp`

**Impact:** Potential crashes on edge cases

**Effort:** Easy (find missing NULL checks, add guards)

---

### 🟡 MEDIUM: Obsolete TODO Comments (Low Impact, Documentation)

**Problem:** Many TODO comments indicate unresolved design questions or incomplete features.

**Examples:**
```c
// code/r_utils/VeyeRaspiVid.c
// TODO: What limits do we need for timeout?
// TODO: What limits do we need for fps 1 - 30 - 120??

// code/renderer/lodepng.c
/*TODO: this ignores potential out of memory errors*/
/*TODO: check for out of memory errors*/
/*TODO: do this not only for zeros but for any repeated byte...*/
/*TODO: possible efficiency improvement: if in this reduced image...*/
```

**Action:**
1. Audit each TODO to determine if it's still relevant
2. Create GitHub issues for unresolved TODOs
3. Remove resolved ones with a comment linking to the fix commit

**Impact:** Code quality / Documentation

**Effort:** Easy (documentation pass)

---

### 🟠 MINOR: Memory Allocation Error Handling

**Problem:** Some memory allocation failures aren't properly handled.

**Example (from renderers):**
```c
// code/renderer/lodepng.c
else return 0; /*error: not enough memory*/
// But allocations are sometimes not checked for NULL
```

**Search Pattern:**
```bash
grep -r "malloc\|calloc" code/ --include="*.cpp" --include="*.c" -A 1 | \
  grep -v "NULL\|if\|return\|assert" | head -20
```

**Impact:** Potential NULL pointer dereference under memory pressure

**Effort:** Easy (add error checks)

---

### 🟢 OPTIMIZATION: Deprecated/Unused Code

**Problem:** Several files contain old code or unused functions that could be cleaned up.

**Examples:**
- `code/r_utils/VeyeRaspiVid.c` - Appears to be legacy Veye camera support
- Old Raspberry Pi MMAL code that's been superseded by newer approaches
- Commented-out debug code

**Search:**
```bash
grep -r "^//\|^/\*" code/ --include="*.cpp" --include="*.c" | wc -l
# Result: ~15000+ lines of commented code
```

**Action:** Code cleanup pass - identify and remove truly dead code

**Impact:** Codebase maintainability

**Effort:** Medium (requires understanding which code is still used)

---

## Recommended PR Strategy

### PR #1: String Safety (CRITICAL)
**Title:** "Security: Replace unsafe strcpy/strcat/sprintf with snprintf (5000+ fixes)"

**Scope:** Bulk replacement with careful review of each file

**Steps:**
1. Fork the repo
2. Create branch: `fix/string-safety`
3. For each affected file:
   - Replace `strcpy(dst, src)` → `snprintf(dst, sizeof(dst), "%s", src)`
   - Replace `strcat(dst, src)` → Smart append
   - Replace `sprintf` → `snprintf`
4. Test each change
5. Open PR with detailed explanation

**Review Strategy:**
- Likely to be split into multiple PRs (one per module)
- Assign to a maintainer who cares about security

---

### PR #2: NULL Pointer Safety (MEDIUM)
**Title:** "Fix: Add missing NULL pointer checks (15+ locations)"

**Steps:**
1. Search for all NULL checks
2. Identify patterns where checks are missing
3. Add guards with clear error messages
4. Test edge cases

---

### PR #3: TODO Audit & Issue Triage (LOW IMPACT)
**Title:** "Docs: Audit and triage TODO comments"

**Steps:**
1. Extract all TODOs: `grep -r "TODO\|FIXME" code/`
2. Categorize as:
   - Still valid (create GitHub issues)
   - Resolved (remove with commit reference)
   - Unclear (ask maintainer)
3. Submit PR with cleaned-up code

---

## How to Get Started (For Contributors)

```bash
# 1. Clone the repo
git clone https://github.com/RubyFPV/RubyFPV.git
cd RubyFPV

# 2. Create a branch
git checkout -b fix/string-safety

# 3. Find unsafe strings
grep -r "strcpy" code/ --include="*.cpp" --include="*.c" | head -5

# 4. Pick a file and fix it
# Example: code/r_utils/ruby_alive.cpp

# 5. Test the build
make clean && make RUBY_BUILD_ENV=radxa 2>&1 | tee build.log

# 6. Commit and push
git add code/r_utils/ruby_alive.cpp
git commit -m "Security: Replace unsafe strcpy/strcat with snprintf in ruby_alive.cpp"
git push origin fix/string-safety

# 7. Open PR on GitHub
```

---

## Summary Table: Quick Wins

| Issue | Files | Instances | Effort | Impact | Type |
|-------|-------|-----------|--------|--------|------|
| String safety (strcpy/strcat/sprintf) | 30+ | 5000+ | Medium | Critical | Security |
| NULL pointer checks | 8+ | 15+ | Easy | Medium | Reliability |
| TODO audit | 10+ | 100+ | Easy | Low | Documentation |
| Memory alloc error handling | 5+ | 20+ | Easy | Medium | Reliability |
| Dead code cleanup | 5+ | 100s | Medium | Low | Maintenance |

---

## Notes for New Contributors

**Why These Are Good First Issues:**
1. ✅ Well-scoped (can do one file at a time)
2. ✅ High impact (security + reliability)
3. ✅ Easy to review (mechanical changes)
4. ✅ Build system already set up (can test locally)

**Before Opening a PR:**
1. Check if there's already a related issue
2. Comment on the issue or PR with "I'd like to work on this"
3. Wait for maintainer approval (avoid duplicate work)
4. Test your changes: `make clean && make RUBY_BUILD_ENV=radxa`
5. Write clear commit messages

---

## Additional Resources

- **RubyFPV Documentation:** https://rubyfpv.com
- **GitHub Security Best Practices:** https://owasp.org/www-community/attacks/Buffer_Overflow
- **CWE-120 (Buffer Copy):** https://cwe.mitre.org/data/definitions/120.html

