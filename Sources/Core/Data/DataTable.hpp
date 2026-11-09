#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hm2 {
// Original QTable token rules, not a generic strict TSV schema.
class DataTable {
    std::vector<std::uint8_t> encoded_;
    std::string decoded_;
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
public:
    static DataTable parse(std::span<const std::uint8_t> encoded);
    static DataTable parseText(std::string_view text);
    [[nodiscard]] const auto& encoded()const{return encoded_;}
    [[nodiscard]] const auto& decoded()const{return decoded_;}
    [[nodiscard]] const auto& columns()const{return columns_;}
    [[nodiscard]] const auto& rows()const{return rows_;}
    [[nodiscard]] std::size_t rowCount()const{return rows_.size();}
    // nullopt (not present) differs from a stored empty string.
    [[nodiscard]] std::optional<std::string_view> value(std::size_t row,std::string_view column)const;
    // Inspection helper (first matching row), not an original QTable API.
    // Each original consumer builds its own ID dictionary when required.
    [[nodiscard]] std::optional<std::size_t> findRow(std::string_view key)const;
};
}
