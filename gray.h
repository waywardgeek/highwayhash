#include <cstdint>

class Gray {
  public:
    static const size_t kMaxStackSize = 32;

    Gray(): value_(0), pos_(0), last_bit_flipped_(0), even_parity_(true) {}

    inline uint32_t increment() {
        if (even_parity_) {
            if (value_ == 0 or stack_[pos_-1] != 0) {
                push(0);
            } else {
                pop();
            }
            last_bit_flipped_ = 0;
        } else {
            uint32_t last_one_bit_ = pop();
            last_bit_flipped_ = last_one_bit_ + 1;
            if (pos_ == 0 or stack_[pos_-1] != last_bit_flipped_) {
                push(last_bit_flipped_);
            } else {
                pop();
            }
            push(last_one_bit_);
        }
        value_ ^= 1 << last_bit_flipped_;
        even_parity_ = !even_parity_;
        return value_;
    }

    inline uint32_t pop() {
        return stack_[--pos_];
    }

    inline uint32_t getValue() { return value_; }
    inline uint32_t getLastBitFlipped() { return last_bit_flipped_; }

    inline void push(uint32_t bit_pos) {
        stack_[pos_++] = bit_pos;
    }

  private:
    uint32_t value_;
    uint32_t pos_;
    uint32_t last_bit_flipped_;
    uint32_t stack_[kMaxStackSize];
    bool even_parity_;
};
