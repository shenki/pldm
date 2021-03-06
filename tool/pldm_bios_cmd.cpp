#include "pldm_bios_cmd.hpp"

#include "bios_utils.hpp"
#include "pldm_cmd_helper.hpp"
#include "utils.hpp"

#include <map>
#include <optional>

#include "libpldm/bios_table.h"
#include "libpldm/utils.h"

namespace pldmtool
{

namespace bios
{

namespace
{

using namespace pldmtool::helper;
using namespace pldm::bios::utils;

std::vector<std::unique_ptr<CommandInterface>> commands;

const std::map<const char*, pldm_bios_table_types> pldmBIOSTableTypes{
    {"StringTable", PLDM_BIOS_STRING_TABLE},
    {"AttributeTable", PLDM_BIOS_ATTR_TABLE},
    {"AttributeValueTable", PLDM_BIOS_ATTR_VAL_TABLE},
};

} // namespace

class GetDateTime : public CommandInterface
{
  public:
    ~GetDateTime() = default;
    GetDateTime() = delete;
    GetDateTime(const GetDateTime&) = delete;
    GetDateTime(GetDateTime&&) = default;
    GetDateTime& operator=(const GetDateTime&) = delete;
    GetDateTime& operator=(GetDateTime&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_date_time_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;

        uint8_t seconds, minutes, hours, day, month;
        uint16_t year;
        auto rc =
            decode_get_date_time_resp(responsePtr, payloadLength, &cc, &seconds,
                                      &minutes, &hours, &day, &month, &year);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        std::cout << "Date & Time : " << std::endl;
        std::cout << "YYYY-MM-DD HH:MM:SS - ";
        std::cout << bcd2dec16(year);
        std::cout << "-";
        setWidth(month);
        std::cout << "-";
        setWidth(day);
        std::cout << " ";
        setWidth(hours);
        std::cout << ":";
        setWidth(minutes);
        std::cout << ":";
        setWidth(seconds);
        std::cout << std::endl;
    }

  private:
    void setWidth(uint8_t data)
    {
        std::cout << std::setfill('0') << std::setw(2)
                  << static_cast<uint32_t>(bcd2dec8(data));
    }
};

class SetDateTime : public CommandInterface
{
  public:
    ~SetDateTime() = default;
    SetDateTime() = delete;
    SetDateTime(const SetDateTime&) = delete;
    SetDateTime(SetDateTime&&) = default;
    SetDateTime& operator=(const SetDateTime&) = delete;
    SetDateTime& operator=(SetDateTime&&) = default;

    explicit SetDateTime(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", tmData,
                        "set date time data\n"
                        "eg: YYYYMMDDHHMMSS")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        sizeof(struct pldm_set_date_time_req));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        uint8_t hours = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;

        if (!uintToDate(tmData, &year, &month, &day, &hours, &minutes,
                        &seconds))
        {
            std::cerr << "decode date Error: "
                      << "tmData=" << tmData << std::endl;

            return {PLDM_ERROR_INVALID_DATA, requestMsg};
        }

        auto rc = encode_set_date_time_req(
            instanceId, seconds, minutes, hours, day, month, year, request,
            sizeof(struct pldm_set_date_time_req));

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        auto rc = decode_set_date_time_resp(responsePtr, payloadLength,
                                            &completionCode);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }

        std::cout << "SetDateTime: SUCCESS" << std::endl;
    }

  private:
    uint64_t tmData;
};

class GetBIOSTable : public CommandInterface
{
  public:
    ~GetBIOSTable() = default;
    GetBIOSTable() = delete;
    GetBIOSTable(const GetBIOSTable&) = delete;
    GetBIOSTable(GetBIOSTable&&) = default;
    GetBIOSTable& operator=(const GetBIOSTable&) = delete;
    GetBIOSTable& operator=(GetBIOSTable&&) = default;

    using Table = std::vector<uint8_t>;

    explicit GetBIOSTable(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-t,--type", pldmBIOSTableType, "pldm bios table type")
            ->required()
            ->transform(
                CLI::CheckedTransformer(pldmBIOSTableTypes, CLI::ignore_case));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        return {PLDM_ERROR, {}};
    }
    void parseResponseMsg(pldm_msg*, size_t) override
    {
    }

    std::optional<Table> getBIOSTable(pldm_bios_table_types tableType)
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_BIOS_TABLE_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_bios_table_req(instanceId, 0, PLDM_GET_FIRSTPART,
                                            tableType, request);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Encode GetBIOSTable Error, tableType=," << tableType
                      << " ,rc=" << rc << std::endl;
            return std::nullopt;
        }
        std::vector<uint8_t> responseMsg;
        rc = pldmSendRecv(requestMsg, responseMsg);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "PLDM: Communication Error, rc =" << rc << std::endl;
            return std::nullopt;
        }

        uint8_t cc = 0, transferFlag = 0;
        uint32_t nextTransferHandle = 0;
        size_t bios_table_offset;
        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsg.data());
        auto payloadLength = responseMsg.size() - sizeof(pldm_msg_hdr);

        rc = decode_get_bios_table_resp(responsePtr, payloadLength, &cc,
                                        &nextTransferHandle, &transferFlag,
                                        &bios_table_offset);

        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "GetBIOSTable Response Error: tableType=" << tableType
                      << ", rc=" << rc << ", cc=" << (int)cc << std::endl;
            return std::nullopt;
        }
        auto tableData =
            reinterpret_cast<char*>((responsePtr->payload) + bios_table_offset);
        auto tableSize = payloadLength - sizeof(nextTransferHandle) -
                         sizeof(transferFlag) - sizeof(cc);
        return std::make_optional<Table>(tableData, tableData + tableSize);
    }

    void exec() override
    {

        switch (pldmBIOSTableType)
        {
            case PLDM_BIOS_STRING_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                decodeStringTable(stringTable);
                break;
            }
            case PLDM_BIOS_ATTR_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

                decodeAttributeTable(attrTable, stringTable);
            }
            break;
            case PLDM_BIOS_ATTR_VAL_TABLE:
            {
                auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
                auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
                auto attrValTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

                decodeAttributeValueTable(attrValTable, attrTable, stringTable);
                break;
            }
        }
    }

  private:
    pldm_bios_table_types pldmBIOSTableType;

    static inline const std::map<pldm_bios_attribute_type, const char*>
        attrTypeMap = {
            {PLDM_BIOS_ENUMERATION, "BIOSEnumeration"},
            {PLDM_BIOS_ENUMERATION_READ_ONLY, "BIOSEnumerationReadOnly"},
            {PLDM_BIOS_STRING, "BIOSString"},
            {PLDM_BIOS_STRING_READ_ONLY, "BIOSStringReadOnly"},
            {PLDM_BIOS_PASSWORD, "BIOSPassword"},
            {PLDM_BIOS_PASSWORD_READ_ONLY, "BIOSPasswordReadOnly"},
            {PLDM_BIOS_INTEGER, "BIOSInteger"},
            {PLDM_BIOS_INTEGER_READ_ONLY, "BIOSIntegerReadOnly"},

        };

    std::string decodeStringFromStringEntry(
        const pldm_bios_string_table_entry* stringEntry)
    {
        auto strLength =
            pldm_bios_table_string_entry_decode_string_length(stringEntry);
        std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
        pldm_bios_table_string_entry_decode_string(stringEntry, buffer.data(),
                                                   buffer.size());

        return std::string(buffer.data(), buffer.data() + strLength);
    }

    std::string displayStringHandle(uint16_t handle,
                                    const std::optional<Table>& stringTable)
    {
        std::string displayString = std::to_string(handle);
        if (!stringTable)
        {
            return displayString;
        }
        auto stringEntry = pldm_bios_table_string_find_by_handle(
            stringTable->data(), stringTable->size(), handle);
        if (stringEntry == nullptr)
        {
            return displayString;
        }

        return displayString + "(" + decodeStringFromStringEntry(stringEntry) +
               ")";
    }

    std::string displayEnumValueByIndex(uint16_t attrHandle, uint8_t index,
                                        const std::optional<Table>& attrTable,
                                        const std::optional<Table>& stringTable)
    {
        std::string displayString;
        if (!attrTable)
        {
            return displayString;
        }

        auto attrEntry = pldm_bios_table_attr_find_by_handle(
            attrTable->data(), attrTable->size(), attrHandle);
        if (attrEntry == nullptr)
        {
            return displayString;
        }
        auto pvNum = pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry);
        std::vector<uint16_t> pvHandls(pvNum);
        pldm_bios_table_attr_entry_enum_decode_pv_hdls(
            attrEntry, pvHandls.data(), pvHandls.size());
        return displayStringHandle(pvHandls[index], stringTable);
    }

    void decodeStringTable(const std::optional<Table>& stringTable)
    {
        std::cout << "Parsed Response Msg: " << std::endl;
        if (!stringTable)
        {
            std::cerr << "GetBIOSStringTable Error" << std::endl;
            return;
        }
        std::cout << "PLDM StringTable: " << std::endl;
        std::cout << "BIOSStringHandle : BIOSString" << std::endl;

        for (auto tableEntry : BIOSTableIter<PLDM_BIOS_STRING_TABLE>(
                 stringTable->data(), stringTable->size()))
        {
            auto strHandle =
                pldm_bios_table_string_entry_decode_handle(tableEntry);
            auto strTableData = decodeStringFromStringEntry(tableEntry);
            std::cout << strHandle << " : " << strTableData << std::endl;
        }
    }
    void decodeAttributeTable(const std::optional<Table>& attrTable,
                              const std::optional<Table>& stringTable)
    {
        std::cout << "Parsed Response Msg: " << std::endl;
        if (!stringTable)
        {
            std::cerr << "GetBIOSAttributeTable Error" << std::endl;
            return;
        }
        std::cout << "PLDM AttributeTable: " << std::endl;
        for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(
                 attrTable->data(), attrTable->size()))
        {
            auto attrHandle =
                pldm_bios_table_attr_entry_decode_attribute_handle(entry);
            auto attrNameHandle =
                pldm_bios_table_attr_entry_decode_string_handle(entry);
            auto attrType = static_cast<pldm_bios_attribute_type>(
                pldm_bios_table_attr_entry_decode_attribute_type(entry));
            std::cout << "AttributeHandle: " << attrHandle
                      << ", AttributeNameHandle: "
                      << displayStringHandle(attrNameHandle, stringTable)
                      << std::endl;
            std::cout << "\tAttributeType: " << attrTypeMap.at(attrType)
                      << std::endl;
            switch (attrType)
            {
                case PLDM_BIOS_ENUMERATION:
                case PLDM_BIOS_ENUMERATION_READ_ONLY:
                {
                    auto pvNum =
                        pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
                    std::vector<uint16_t> pvHandls(pvNum);
                    pldm_bios_table_attr_entry_enum_decode_pv_hdls(
                        entry, pvHandls.data(), pvHandls.size());
                    auto defNum =
                        pldm_bios_table_attr_entry_enum_decode_def_num(entry);
                    std::vector<uint8_t> defIndices(defNum);
                    pldm_bios_table_attr_entry_enum_decode_def_indices(
                        entry, defIndices.data(), defIndices.size());
                    std::cout << "\tNumberOfPossibleValues: " << (int)pvNum
                              << std::endl;

                    for (size_t i = 0; i < pvHandls.size(); i++)
                    {
                        std::cout
                            << "\t\tPossibleValueStringHandle"
                            << "[" << i << "] = "
                            << displayStringHandle(pvHandls[i], stringTable)
                            << std::endl;
                    }
                    std::cout << "\tNumberOfDefaultValues: " << (int)defNum
                              << std::endl;
                    for (size_t i = 0; i < defIndices.size(); i++)
                    {
                        std::cout << "\t\tDefaultValueStringHandleIndex"
                                  << "[" << i << "] = " << (int)defIndices[i]
                                  << ", StringHandle = "
                                  << displayStringHandle(
                                         pvHandls[defIndices[i]], stringTable)
                                  << std::endl;
                    }
                    break;
                }
                case PLDM_BIOS_INTEGER:
                case PLDM_BIOS_INTEGER_READ_ONLY:
                {
                    uint64_t lower, upper, def;
                    uint32_t scalar;
                    pldm_bios_table_attr_entry_integer_decode(
                        entry, &lower, &upper, &scalar, &def);
                    std::cout << "\tLowerBound: " << lower << std::endl
                              << "\tUpperBound: " << upper << std::endl
                              << "\tScalarIncrement: " << scalar << std::endl
                              << "\tDefaultValue: " << def << std::endl;
                    break;
                }
                case PLDM_BIOS_STRING:
                case PLDM_BIOS_STRING_READ_ONLY:
                {
                    auto strType =
                        pldm_bios_table_attr_entry_string_decode_string_type(
                            entry);
                    auto min =
                        pldm_bios_table_attr_entry_string_decode_min_length(
                            entry);
                    auto max =
                        pldm_bios_table_attr_entry_string_decode_max_length(
                            entry);
                    auto def =
                        pldm_bios_table_attr_entry_string_decode_def_string_length(
                            entry);
                    std::vector<char> defString(def + 1);
                    pldm_bios_table_attr_entry_string_decode_def_string(
                        entry, defString.data(), defString.size());
                    std::cout
                        << "\tStringType: 0x" << std::hex << std::setw(2)
                        << std::setfill('0') << (int)strType << std::dec
                        << std::setw(0) << std::endl
                        << "\tMinimumStringLength: " << (int)min << std::endl
                        << "\tMaximumStringLength: " << (int)max << std::endl
                        << "\tDefaultStringLength: " << (int)def << std::endl
                        << "\tDefaultString: " << defString.data() << std::endl;
                    break;
                }
                case PLDM_BIOS_PASSWORD:
                case PLDM_BIOS_PASSWORD_READ_ONLY:
                    std::cout << "Password attribute: Not Supported"
                              << std::endl;
            }
        }
    }
    void decodeAttributeValueTable(const std::optional<Table>& attrValTable,
                                   const std::optional<Table>& attrTable,
                                   const std::optional<Table>& stringTable)
    {
        std::cout << "Parsed Response Msg: " << std::endl;
        if (!attrValTable)
        {
            std::cerr << "GetBIOSAttributeValueTable Error" << std::endl;
            return;
        }
        std::cout << "PLDM AttributeValueTable: " << std::endl;
        for (auto tableEntry : BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(
                 attrValTable->data(), attrValTable->size()))
        {
            auto attrHandle =
                pldm_bios_table_attr_value_entry_decode_attribute_handle(
                    tableEntry);
            auto attrType = static_cast<pldm_bios_attribute_type>(
                pldm_bios_table_attr_value_entry_decode_attribute_type(
                    tableEntry));
            std::cout << "AttributeHandle: " << attrHandle << std::endl;
            std::cout << "\tAttributeType: " << attrTypeMap.at(attrType)
                      << std::endl;
            switch (attrType)
            {
                case PLDM_BIOS_ENUMERATION:
                case PLDM_BIOS_ENUMERATION_READ_ONLY:
                {

                    auto count =
                        pldm_bios_table_attr_value_entry_enum_decode_number(
                            tableEntry);
                    std::vector<uint8_t> handles(count);
                    pldm_bios_table_attr_value_entry_enum_decode_handles(
                        tableEntry, handles.data(), handles.size());
                    std::cout << "\tNumberOfCurrentValues: " << (int)count
                              << std::endl;
                    for (size_t i = 0; i < handles.size(); i++)
                    {
                        std::cout
                            << "\tCurrentValueStringHandleIndex[" << i
                            << "] = " << (int)handles[i] << ", StringHandle = "
                            << displayEnumValueByIndex(attrHandle, handles[i],
                                                       attrTable, stringTable)
                            << std::endl;
                    }
                    break;
                }
                case PLDM_BIOS_INTEGER:
                case PLDM_BIOS_INTEGER_READ_ONLY:
                {
                    auto cv =
                        pldm_bios_table_attr_value_entry_integer_decode_cv(
                            tableEntry);
                    std::cout << "\tCurrentValue: " << cv << std::endl;
                    break;
                }
                case PLDM_BIOS_STRING:
                case PLDM_BIOS_STRING_READ_ONLY:
                {
                    auto stringLength =
                        pldm_bios_table_attr_value_entry_string_decode_length(
                            tableEntry);
                    variable_field currentString;
                    pldm_bios_table_attr_value_entry_string_decode_string(
                        tableEntry, &currentString);
                    std::cout << "\tCurrentStringLength: " << stringLength
                              << std::endl
                              << "\tCurrentString: "
                              << std::string(reinterpret_cast<const char*>(
                                                 currentString.ptr),
                                             currentString.length)
                              << std::endl;

                    break;
                }
                case PLDM_BIOS_PASSWORD:
                case PLDM_BIOS_PASSWORD_READ_ONLY:
                    std::cout << "Password attribute: Not Supported"
                              << std::endl;
            }
        }
    }
};

void registerCommand(CLI::App& app)
{
    auto bios = app.add_subcommand("bios", "bios type command");
    bios->require_subcommand(1);
    auto getDateTime = bios->add_subcommand("GetDateTime", "get date time");
    commands.push_back(
        std::make_unique<GetDateTime>("bios", "GetDateTime", getDateTime));

    auto setDateTime =
        bios->add_subcommand("SetDateTime", "set host date time");
    commands.push_back(
        std::make_unique<SetDateTime>("bios", "setDateTime", setDateTime));

    auto getBIOSTable = bios->add_subcommand("GetBIOSTable", "get bios table");
    commands.push_back(
        std::make_unique<GetBIOSTable>("bios", "GetBIOSTable", getBIOSTable));
}

} // namespace bios

} // namespace pldmtool
