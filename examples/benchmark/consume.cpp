// Kept in a separate translation unit so timed results escape the caller.
namespace {
const void* volatile sink = nullptr;
}

void consume(const void* value) {
    sink = value;
}
