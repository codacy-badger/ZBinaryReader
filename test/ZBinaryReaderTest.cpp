#include "ZBinaryReader.hpp"
#include "gtest/gtest.h"

using namespace ZBinaryReader;

namespace {

#define UNUSED(v) static_cast<void>(v)

template <class Dst>
Dst safeCharArrayCast(const char* src) noexcept {
    static_assert(std::is_trivially_copyable_v<Dst>);

    Dst dst;
    std::memcpy(&dst, src, sizeof(Dst));
    return dst;
}

struct TriviallyCopyableStruct {
    int a;
    int b;
};

static_assert(std::is_trivially_copyable<TriviallyCopyableStruct>::value);

bool operator==(const TriviallyCopyableStruct& s0, const TriviallyCopyableStruct& s1) {
    return (s0.a == s1.a) && (s0.b == s1.b);
}

constexpr char testData[] = {
    static_cast<char>(0x20), static_cast<char>(0xA0), static_cast<char>(0x24),
    static_cast<char>(0x29), static_cast<char>(0xC3), static_cast<char>(0x18),
    static_cast<char>(0xCF), static_cast<char>(0x28), static_cast<char>(0x23),
    static_cast<char>(0x9F), static_cast<char>(0x24), static_cast<char>(0x29),
    static_cast<char>(0xC3), static_cast<char>(0x18), static_cast<char>(0xFD),
    static_cast<char>(0xBE), static_cast<char>(0x1F), static_cast<char>(0x85),
    static_cast<char>(0xEB), static_cast<char>(0x51), static_cast<char>(0xB8),
    static_cast<char>(0x1E), static_cast<char>(0x09), static_cast<char>(0x40),
    static_cast<char>(0x7B), static_cast<char>(0x14), static_cast<char>(0x2E),
    static_cast<char>(0x40), static_cast<char>(0x54), static_cast<char>(0x65),
    static_cast<char>(0x73), static_cast<char>(0x74), static_cast<char>(0x74),
    static_cast<char>(0x73), static_cast<char>(0x65), static_cast<char>(0x54),
    static_cast<char>(0x54), static_cast<char>(0x65), static_cast<char>(0x73),
    static_cast<char>(0x74), static_cast<char>(0x00), static_cast<char>(0x74),
    static_cast<char>(0x73), static_cast<char>(0x65), static_cast<char>(0x54),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x00),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x00),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x00),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x00),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x00),
    static_cast<char>(0x00), static_cast<char>(0x00), static_cast<char>(0x54),
    static_cast<char>(0x65), static_cast<char>(0x44)
};

std::filesystem::path getTmpTestDataFilePath() {
    return std::filesystem::temp_directory_path() / "testData.bin";
}

template <typename Source>
class BinaryReaderTestFixture : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if constexpr(std::is_same_v<Source, FileSource>) {
            const auto path = getTmpTestDataFilePath();
            if(path.empty())
                throw std::runtime_error("Failed to contruct temp path for testData file");

            std::ofstream ofs(path.generic_string(), std::ios::binary | std::ios::out);
            ofs.exceptions(std::ios::failbit | std::ios::badbit);
            if(!ofs.is_open())
                throw std::runtime_error("Failed to open temp testData file");
            ofs.write(testData, sizeof(testData));
            ofs.close();
        }
    }

    static void TearDownTestSuite() {
        const auto path = getTmpTestDataFilePath();
        if(!path.empty() && std::filesystem::exists(path))
            std::filesystem::remove(path);
    }

    void SetUp() override {

        if constexpr(std::is_same_v<Source, BufferSource>) {
            br = std::make_unique<BinaryReader>(testData, sizeof(testData));
        } else if constexpr(std::is_same_v<Source, FileSource>)
            br = std::make_unique<BinaryReader>(getTmpTestDataFilePath());
        else
            throw; // unreachable;
    }

    template <typename ReadT>
    void validatedTrivialLERead() {
        const auto off = br->tell();
        ASSERT_EQ(br->template read<ReadT>(), safeCharArrayCast<ReadT>(&testData[off]));
        ASSERT_EQ(br->tell(), off + sizeof(ReadT));
    }

    template <typename ReadT>
    void validatedTrivialBERead() {
        const auto off = br->tell();

        char bytes[sizeof(ReadT)];
        std::memcpy(bytes, &testData[off], sizeof(ReadT));
        std::reverse(std::begin(bytes), std::end(bytes));

        ASSERT_EQ((br->template read<ReadT, BinaryReader::Endianness::BE>()), safeCharArrayCast<ReadT>(bytes));
        ASSERT_EQ(br->tell(), off + sizeof(ReadT));
    }

    template <typename ReadT, int64_t len>
    void validatedLEArrayRead() {
        const auto off = br->tell();

        ReadT arr[len];
        br->read(arr, len);
        for(int i = 0; i < len; ++i)
            ASSERT_EQ(arr[i], safeCharArrayCast<ReadT>(&testData[off + sizeof(ReadT) * i]));

        ASSERT_EQ(br->tell(), off + sizeof(ReadT) * len);
    }


    template <typename ReadT, int64_t len>
    void validatedBEArrayRead() {
        const auto off = br->tell();

        ReadT arr[len];
        br->read<ReadT, BinaryReader::Endianness::BE>(arr, len);
        for(int i = 0; i < len; ++i) {
            ReadT val = safeCharArrayCast<ReadT>(&testData[off + sizeof(ReadT) * i]);
            std::reverse(reinterpret_cast<char*>(&val), reinterpret_cast<char*>(&val) + sizeof(ReadT));
            ASSERT_EQ(arr[i], val);
        }

        ASSERT_EQ(br->tell(), off + sizeof(ReadT) * len);
    }

    void validatedSeekTell(int seekOffset) {
        br->seek(seekOffset);
        ASSERT_EQ(br->tell(), seekOffset);
    }

    std::unique_ptr<BinaryReader> br;
};

#if GTEST_HAS_TYPED_TEST

using testing::Types;

typedef Types<FileSource, BufferSource> Implementations;

TYPED_TEST_SUITE(BinaryReaderTestFixture, Implementations);

TYPED_TEST(BinaryReaderTestFixture, SourceSize) {
    ASSERT_EQ(this->br->size(), sizeof(testData));
}

TYPED_TEST(BinaryReaderTestFixture, SeekTell) {
    // Seek 0->0
    this->validatedSeekTell(0);

    // Seek to valid position.
    this->validatedSeekTell(10);

    // Seek to end
    this->validatedSeekTell(sizeof(testData));

    // Seek past end
    this->validatedSeekTell(sizeof(testData) + 1);

    // TODO: Define behaviour for seeking negative values
    // seek to neg offset
    // br.seek(-1);
    // ASSERT_EQ(br.tell(), 0);
}

TYPED_TEST(BinaryReaderTestFixture, ReadPrimitiveTypes) {
    this->br->seek(0);

    this->template validatedTrivialLERead<int64_t>();
    this->template validatedTrivialLERead<int32_t>();
    this->template validatedTrivialLERead<int16_t>();
    this->template validatedTrivialLERead<int8_t>();
    this->template validatedTrivialLERead<uint64_t>();
    this->template validatedTrivialLERead<uint32_t>();
    this->template validatedTrivialLERead<uint16_t>();
    this->template validatedTrivialLERead<uint8_t>();
    this->template validatedTrivialLERead<double>();
    this->template validatedTrivialLERead<float>();

    this->br->seek(0);
    this->template validatedTrivialBERead<int64_t>();
    this->template validatedTrivialBERead<int32_t>();
    this->template validatedTrivialBERead<int16_t>();
    this->template validatedTrivialBERead<int8_t>();
    this->template validatedTrivialBERead<uint64_t>();
    this->template validatedTrivialBERead<uint32_t>();
    this->template validatedTrivialBERead<uint16_t>();
    this->template validatedTrivialBERead<uint8_t>();
    this->template validatedTrivialBERead<double>();
    this->template validatedTrivialBERead<float>();
}

TYPED_TEST(BinaryReaderTestFixture, ReadArrays) {
    this->br->seek(0);
    this->template validatedLEArrayRead<int64_t, 2>();
    this->br->seek(0);
    this->template validatedLEArrayRead<int32_t, 4>();
    this->br->seek(0);
    this->template validatedLEArrayRead<int16_t, 8>();
    this->br->seek(0);
    this->template validatedLEArrayRead<int8_t, 16>();
    this->br->seek(0);
    this->template validatedLEArrayRead<uint64_t, 2>();
    this->br->seek(0);
    this->template validatedLEArrayRead<uint32_t, 4>();
    this->br->seek(0);
    this->template validatedLEArrayRead<uint16_t, 8>();
    this->br->seek(0);
    this->template validatedLEArrayRead<uint8_t, 16>();
    this->br->seek(0);
    this->template validatedLEArrayRead<double, 2>();
    this->br->seek(0);
    this->template validatedLEArrayRead<float, 4>();

    this->br->seek(0);
    this->template validatedBEArrayRead<int64_t, 2>();
    this->br->seek(0);
    this->template validatedBEArrayRead<int32_t, 4>();
    this->br->seek(0);
    this->template validatedBEArrayRead<int16_t, 8>();
    this->br->seek(0);
    this->template validatedBEArrayRead<int8_t, 16>();
    this->br->seek(0);
    this->template validatedBEArrayRead<uint64_t, 2>();
    this->br->seek(0);
    this->template validatedBEArrayRead<uint32_t, 4>();
    this->br->seek(0);
    this->template validatedBEArrayRead<uint16_t, 8>();
    this->br->seek(0);
    this->template validatedBEArrayRead<uint8_t, 16>();
    this->br->seek(0);
    this->template validatedBEArrayRead<double, 2>();
    this->br->seek(0);
    this->template validatedBEArrayRead<float, 4>();
}

TYPED_TEST(BinaryReaderTestFixture, ReadLEArrayOfStruct) {
    this->br->seek(0);
    this->template validatedLEArrayRead<TriviallyCopyableStruct, 2>();
}

TYPED_TEST(BinaryReaderTestFixture, ReadStrings) {
    constexpr int stringsOffset = 0x1C;
    constexpr int testStrLen = 4;

    this->br->seek(stringsOffset);

    ASSERT_STREQ(this->br->template readString<testStrLen>().c_str(), "Test");
    ASSERT_STREQ((this->br->template readString<4, BinaryReader::Endianness::LE>().c_str()),
                 "Test");
    ASSERT_STREQ(this->br->readCString().c_str(), "Test");
    ASSERT_STREQ(this->br->template readCString<BinaryReader::Endianness::LE>().c_str(), "Test");

    this->br->seek(stringsOffset);
    ASSERT_STREQ(this->br->readString(testStrLen).c_str(), "Test");
}

// Try to read fixed size string which contains null chars
TYPED_TEST(BinaryReaderTestFixture, ReadInvalidString) {
    this->br->seek(0x20);
    ASSERT_THROW(UNUSED(this->br->template readString<0x0A>()), std::runtime_error);
}

TYPED_TEST(BinaryReaderTestFixture, ReadStringPastEnd) {
    this->br->seek(0x3E);
    ASSERT_THROW(UNUSED(this->br->template readString<0x10>()), std::runtime_error);
}

TYPED_TEST(BinaryReaderTestFixture, ReadCStringPastEnd) {
    this->br->seek(0x3E);
    ASSERT_THROW(UNUSED(this->br->readCString()), std::runtime_error);
}

TYPED_TEST(BinaryReaderTestFixture, ReadCopyableType) {
    this->template validatedTrivialLERead<TriviallyCopyableStruct>();
}

TYPED_TEST(BinaryReaderTestFixture, Sink) {
    // Sink fundamental type
    using FT = int;
    const auto begin = this->br->tell();
    this->br->template sink<FT>();
    ASSERT_EQ(this->br->tell(), begin + sizeof(FT));

    // Sink compound type
    using CT = TriviallyCopyableStruct;
    this->br->seek(begin);
    this->br->template sink<CT>();
    ASSERT_EQ(this->br->tell(), begin + sizeof(CT));

    // Sink fundamental type array
    constexpr int arrLen = 4;
    this->br->seek(begin);
    this->br->template sink<FT>(arrLen);
    ASSERT_EQ(this->br->tell(), begin + arrLen * sizeof(FT));
}

TYPED_TEST(BinaryReaderTestFixture, Peek) {
    // Return value of peek is not checked as it's implicitly checked as part of br->read testing.

    // Peek fundamental type
    using FT = int;
    const auto begin = this->br->tell();
    UNUSED(this->br->template peek<FT>());
    ASSERT_EQ(this->br->tell(), begin);

    // Peek compound type
    using CT = TriviallyCopyableStruct;
    this->br->seek(begin);
    UNUSED(this->br->template peek<CT>());
    ASSERT_EQ(this->br->tell(), begin);

    // Peek fundamental type array
    constexpr int arrLen = 4;
    FT arr[arrLen]{};

    this->br->seek(begin);
    this->br->peek(arr, arrLen);
    ASSERT_EQ(this->br->tell(), begin);
}


TYPED_TEST(BinaryReaderTestFixture, Alignment) {
    // Align zero
    this->br->align();
    ASSERT_EQ(this->br->tell(), 0);

    // Align already aligned
    this->br->seek(1);
    this->br->template align<1>();
    ASSERT_EQ(this->br->tell(), 1);

    // Align from -1 off
    this->br->align();
    ASSERT_EQ(this->br->tell(), 0x10);

    // Align from 1 off
    this->br->template align<0x11>();
    ASSERT_EQ(this->br->tell(), 0x11);

    // Align to multiple
    this->br->template align<4>();
    ASSERT_EQ(this->br->tell(), 0x14);
}

// Throw if aligning past end of read source
TYPED_TEST(BinaryReaderTestFixture, AlignmentPastEnd) {
    this->br->seek(1);
    ASSERT_THROW(this->br->template align<sizeof(testData) + 1>(), std::runtime_error);
}

TYPED_TEST(BinaryReaderTestFixture, AlignmentZeroPad) {
    // Align already aligned
    this->br->alignZeroPad();
    ASSERT_EQ(this->br->tell(), 0);

    // Align from 'Alignment'-1 off
    this->br->seek(0x31);
    this->br->template alignZeroPad<8>();
    ASSERT_EQ(this->br->tell(), 0x38);

    // Align from 1 off
    this->br->seek(0x2F);
    this->br->alignZeroPad();
    ASSERT_EQ(this->br->tell(), 0x30);
}

// Throw if aligning past end of read source
TYPED_TEST(BinaryReaderTestFixture, AlignmentZeroPadPastEnd) {
    this->br->seek(1);
    ASSERT_THROW(this->br->template alignZeroPad<sizeof(testData) + 1>(),
                 std::runtime_error); // Will throw before checking padding.
}

// Throw if padding contains non-zero values.
TYPED_TEST(BinaryReaderTestFixture, AlignmentZeroPadBadPadding) {
    this->br->seek(1);
    ASSERT_THROW(this->br->alignZeroPad(), std::runtime_error);
}

#endif // GTEST_HAS_TYPED_TEST


class CoverageTrackingSourceTestFixture : public testing::Test {
protected:
    void SetUp() override {
        std::unique_ptr<BufferSource> source =
        std::make_unique<CoverageTrackingSource<BufferSource>>(testData, sizeof(testData));
        br = std::make_unique<BinaryReader>(std::move(source));
    }

    std::unique_ptr<BinaryReader> br;
};

TEST_F(CoverageTrackingSourceTestFixture, CompleteCoverage) {
    br->sink<char>(sizeof(testData));
    ASSERT_TRUE(CoverageTrackingSource<BufferSource>::completeCoverage(br.get()));
}

TEST_F(CoverageTrackingSourceTestFixture, IncompleteCoverage) {
    br->sink<char>(sizeof(testData) - 1);
    ASSERT_FALSE(CoverageTrackingSource<BufferSource>::completeCoverage(br.get()));
}

TEST_F(CoverageTrackingSourceTestFixture, DoubleRead) {
    using T = int;
    UNUSED(br->read<T>());
    br->seek(br->tell() - sizeof(T));
    ASSERT_THROW(UNUSED(br->read<T>()), std::runtime_error);
}

TEST_F(CoverageTrackingSourceTestFixture, DoublePeek) {
    using T = int;
    UNUSED(br->peek<T>());
    br->seek(br->tell() - sizeof(T));
    UNUSED(br->peek<T>());
}

} // namespace