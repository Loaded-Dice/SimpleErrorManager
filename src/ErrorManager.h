#pragma once
#include <Arduino.h>

// ================================================================
//  ErrorManager – X-Macro based error tracking
//
//  Usage:
//
//    // 1) Define error codes in ErrorCodes.def:
//    ERROR_CODE(WIFI_FAILED)
//    ERROR_CODE(SENSOR_TIMEOUT)
//
//    // 2) Optional: Enable string names (before #include):
//    #define INCLUDE_ERROR_NAMES
//
//    // 3) Include ErrorManager:
//    #include "ErrorManager.h"
//
//    // 4) Create instance:
//    ErrorManager EM;
//
//    // 5) Use string names (if enabled):
//    Serial.println(EM.getErrorName(WIFI_FAILED));
//
//  Supports up to 256 error codes (32 bytes bitmask).
// ================================================================

// ── Enum generation via X-Macro ─────────────────────────────────
#define GETCODE(code) static_cast<uint8_t>(code)

#define ERROR_CODE(name) name,
enum ErrCode : uint8_t {
    #include "ErrorCodes.def"
    _ERR_CODE_COUNT_INTERNAL  // automatically calculated
};
#undef ERROR_CODE

struct ErrorEntry {
    uint32_t timestamp = 0;   // millis() at add()
    uint32_t mercyTime = 0;   // millis() + mercy_ms for pending expiration
};

// ── String names for debugging (optional) ────────────────────────
// Enable string names with: #define INCLUDE_ERROR_NAMES before #include "ErrorManager.h"
#ifdef INCLUDE_ERROR_NAMES
    #define ERROR_CODE(name) #name,
    namespace {
        const char* const ERROR_NAMES[] = {
            #include "ErrorCodes.def"
        };
    }
    #undef ERROR_CODE
#endif

// ----------------------------------------------------------------

class ErrorManager {

    static constexpr uint16_t _count = static_cast<uint16_t>(_ERR_CODE_COUNT_INTERNAL);
    static_assert(_count <= 256,
        "Too many error codes! 32x uint8_t mask supports max. 256 entries.");

    // ── Bit manipulation helpers ─────────────────────────────────────
    // idx >> 3  →  byte-index (0-31 for 0-255 errors)
    // idx & 7   →  bit-position within byte (0-7)

    inline void _setBit(uint8_t (&m)[32], uint8_t idx) const { m[idx >> 3] |=  (1 << (idx & 7)); }
    inline void _clrBit(uint8_t (&m)[32], uint8_t idx) const { m[idx >> 3] &= ~(1 << (idx & 7)); }
    inline bool _hasBit(const uint8_t (&m)[32], uint8_t idx) const { return m[idx >> 3] & (1 << (idx & 7));}
    inline bool _anyBit(const uint8_t (&m)[32]) const { for (uint8_t i = 0; i < 32; i++) {
            if (m[i]) return true;
        }
        return false;
    }

public:

    // Add error.
    // mercy_ms == 0 → immediately active
    // mercy_ms  > 0 → pending until mercy period expires
    // Returns false if already active or pending (no overwrite)
    bool add(ErrCode code, uint32_t mercy_ms = 0) {
        uint8_t idx = static_cast<uint8_t>(code);
        if (idx >= _count)              return false;
        if (_hasBit(_activeMask,  idx)) return false;  // already active
        if (_hasBit(_pendingMask, idx)) return false;  // already pending

        ErrorEntry& entry = _errors[idx];
        entry.timestamp   = millis();

        if (mercy_ms == 0) {
            _setBit(_activeMask, idx);
            _activeCount++;
        } else {
            entry.mercyTime = millis() + mercy_ms;
            _setBit(_pendingMask, idx);
            _pendingCount++;
        }
        return true;
    }

    // Clear/acknowledge error.
    // Returns true  if error was active or pending
    // Returns false if error was not set
    bool clear(ErrCode code) {
        uint8_t idx = static_cast<uint8_t>(code);
        if (idx >= _count) return false;

        bool wasActive  = _hasBit(_activeMask,  idx);
        bool wasPending = _hasBit(_pendingMask, idx);

        if (!wasActive && !wasPending) return false;

        if (wasActive)  { _clrBit(_activeMask,  idx); _activeCount--;  }
        if (wasPending) { _clrBit(_pendingMask, idx); _pendingCount--; }

        return true;
    }

    // Call periodically in error task or loop().
    // Promotes pending errors to active when mercyTime expires.
    void tick() {
        if (!_anyBit(_pendingMask)) return;  // fast guard

        uint32_t now = millis();
        for (uint16_t i = 0; i < _count; i++) {
            if (_hasBit(_pendingMask, i) && now >= _errors[i].mercyTime) {
                _clrBit(_pendingMask, i);  _pendingCount--;
                _setBit(_activeMask,  i);  _activeCount++;
            }
        }
    }

    bool    isActive     (ErrCode code) const { return _hasBit(_activeMask,  static_cast<uint8_t>(code)); }
    bool    isPending    (ErrCode code) const { return _hasBit(_pendingMask, static_cast<uint8_t>(code)); }
    uint8_t getActiveCount ()           const { return _activeCount;  }
    uint8_t getPendingCount()           const { return _pendingCount; }

    void clearAll() {
        for (uint8_t i = 0; i < 32; i++) {
            _activeMask[i] = 0;
            _pendingMask[i] = 0;
        }
        _activeCount  = 0;
        _pendingCount = 0;
    }

    uint32_t getTimestamp(ErrCode code) const {
        return _errors[static_cast<uint8_t>(code)].timestamp;
    }

    // ── Iteration support ──────────────────────────────────────────
    // Example: for (ErrCode code : EM.allErrorCodes()) { ... }
    
    class ErrorCodeIterator {
        uint16_t _index;
        uint16_t _maxCount;
    public:
        ErrorCodeIterator(uint16_t idx, uint16_t maxCount) : _index(idx), _maxCount(maxCount) {}
        ErrCode operator*() const { return static_cast<ErrCode>(_index); }
        ErrorCodeIterator& operator++() { ++_index; return *this; }
        bool operator!=(const ErrorCodeIterator& other) const { 
            return _index != other._index; 
        }
    };

    class ErrorCodeRange {
        uint16_t _count;
    public:
        ErrorCodeRange(uint16_t count) : _count(count) {}
        ErrorCodeIterator begin() const { return ErrorCodeIterator(0, _count); }
        ErrorCodeIterator end() const { return ErrorCodeIterator(_count, _count); }
    };

    ErrorCodeRange allErrorCodes() const {
        return ErrorCodeRange(_count);
    }

    // ── String name support (optional) ──────────────────────────────

    const char* getErrorName(ErrCode code) const {
#ifdef INCLUDE_ERROR_NAMES        
        uint8_t idx = static_cast<uint8_t>(code);
        if (idx >= _count) return "INVALID_ERROR_CODE";
        return ERROR_NAMES[idx];
#else
return "use #define INCLUDE_ERROR_NAMES to use names";
#endif        
    }


private:
    ErrorEntry _errors[_count]  = {};
    uint8_t    _activeMask[32]  = {};  // 256 bits = 32 bytes
    uint8_t    _pendingMask[32] = {};  // 256 bits = 32 bytes
    uint8_t    _activeCount     = 0;
    uint8_t    _pendingCount    = 0;
};