#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace audio_studio {

// Word-level edit counts used to evaluate a hypothesis against a verified
// reference transcript. Insertions are especially useful for detecting words
// Whisper generated that were not spoken.
struct TranscriptionEvaluation {
    std::size_t reference_words{0U};
    std::size_t hypothesis_words{0U};
    std::size_t substitutions{0U};
    std::size_t deletions{0U};
    std::size_t insertions{0U};

    double word_error_rate() const noexcept {
        if (reference_words == 0U) {
            return hypothesis_words == 0U ? 0.0 : 1.0;
        }
        return static_cast<double>(substitutions + deletions + insertions) /
               static_cast<double>(reference_words);
    }
};

namespace transcription_evaluation_detail {

inline std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string token;
    const auto flush = [&words, &token]() {
        if (!token.empty()) {
            words.push_back(token);
            token.clear();
        }
    };

    for (const unsigned char value : text) {
        if (std::isspace(value) != 0) {
            flush();
            continue;
        }
        if (value < 0x80U && std::ispunct(value) != 0) {
            continue;
        }
        token.push_back(value < 0x80U
            ? static_cast<char>(std::tolower(value))
            : static_cast<char>(value));
    }
    flush();
    return words;
}

struct Cell {
    std::size_t cost{0U};
    char operation{'M'};
};

}  // namespace transcription_evaluation_detail

// Computes case- and ASCII-punctuation-insensitive word error counts using
// Levenshtein alignment. UTF-8 bytes are preserved for non-ASCII text; this
// deliberately avoids locale-dependent transliteration or accent folding.
inline TranscriptionEvaluation evaluate_transcription(
    const std::string& reference,
    const std::string& hypothesis) {
    using transcription_evaluation_detail::Cell;
    const auto expected = transcription_evaluation_detail::tokenize(reference);
    const auto actual = transcription_evaluation_detail::tokenize(hypothesis);
    const std::size_t rows = expected.size() + 1U;
    const std::size_t columns = actual.size() + 1U;
    std::vector<Cell> matrix(rows * columns);

    const auto at = [&matrix, columns](std::size_t row, std::size_t column) -> Cell& {
        return matrix[row * columns + column];
    };
    for (std::size_t row = 1U; row < rows; ++row) {
        at(row, 0U) = Cell{row, 'D'};
    }
    for (std::size_t column = 1U; column < columns; ++column) {
        at(0U, column) = Cell{column, 'I'};
    }

    for (std::size_t row = 1U; row < rows; ++row) {
        for (std::size_t column = 1U; column < columns; ++column) {
            if (expected[row - 1U] == actual[column - 1U]) {
                at(row, column) = Cell{at(row - 1U, column - 1U).cost, 'M'};
                continue;
            }
            const std::size_t substitution = at(row - 1U, column - 1U).cost + 1U;
            const std::size_t deletion = at(row - 1U, column).cost + 1U;
            const std::size_t insertion = at(row, column - 1U).cost + 1U;
            // Prefer substitution, then deletion, then insertion on ties so
            // the reported categories are deterministic across platforms.
            if (substitution <= deletion && substitution <= insertion) {
                at(row, column) = Cell{substitution, 'S'};
            } else if (deletion <= insertion) {
                at(row, column) = Cell{deletion, 'D'};
            } else {
                at(row, column) = Cell{insertion, 'I'};
            }
        }
    }

    TranscriptionEvaluation result;
    result.reference_words = expected.size();
    result.hypothesis_words = actual.size();
    std::size_t row = expected.size();
    std::size_t column = actual.size();
    while (row > 0U || column > 0U) {
        const char operation = at(row, column).operation;
        if (operation == 'M') {
            --row;
            --column;
        } else if (operation == 'S') {
            ++result.substitutions;
            --row;
            --column;
        } else if (operation == 'D') {
            ++result.deletions;
            --row;
        } else {
            ++result.insertions;
            --column;
        }
    }
    return result;
}

}  // namespace audio_studio
