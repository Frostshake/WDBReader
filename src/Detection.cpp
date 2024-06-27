#include "WDBReader/Detection.hpp"

#include <array>
#include <cassert>
#include <codecvt>
#include <fstream>
#include <regex>

#ifdef WIN32
#include <shobjidl.h>
#endif

namespace WDBReader::Detection {

    class InfoFileReader {
	public:
		using row_t = std::vector<std::string>;

		bool read(std::istream& stream)
        {
            assert(!stream.fail());
            bool header_found = false;

            while (!stream.eof()) {
                std::string line;
                std::getline(stream, line);

                const auto row = Utility::split_string(line, "|");

                if (row.size() > 0) {
                    if (!header_found) {
                        _header = row;
                        header_found = true;
                    }
                    else {
                        if (row.size() != _header.size()) {
                            throw std::runtime_error("Row size doesnt match header size.");
                        }

                        _rows.push_back(row);
                    }
                }
            }

            return header_found;
        }

		const row_t& getHeader() const {
            return _header;
        }

		const std::vector<row_t>& getRows() const {
            return _rows;
        }

	protected:
		row_t _header;
		std::vector<row_t> _rows;
	};


	Detector::Detector(std::initializer_list<strategy_t> init) : 
        _strategies(init)
	{
	}

    Detector Detector::all()
	{
		return Detector{
			Detector::buildFile,
			Detector::filesystemHints
		};
	}

    Detector& Detector::add(Detector::strategy_t&& strategy)
	{
		_strategies.push_back(strategy);
		return *this;
	}

    Detector::result_t Detector::detect(const std::filesystem::path& directory) const
	{
		for (const auto& strategy : _strategies) {
			const auto result = strategy(directory);
			if (result.size() > 0) {
				return result;
			}
		}

		return std::vector<ClientInfo>();
	}

    Detector::result_t Detector::buildFile(const std::filesystem::path& directory)
	{	
		result_t result;

		const std::array<std::string, 1> known_files = {
			".build.info"
		};

		auto find_index = [](const InfoFileReader::row_t& row, auto value) -> int64_t {
			auto it = std::find(row.cbegin(), row.cend(), value);
			if (it != row.cend()) {
				return it - row.cbegin();
			}

			return -1;
		};

		const std::regex locale_regex{ "([a-z]{2}[A-Z]{2})\\stext\\?" };

		for (const auto& file_name : known_files) {
			try {
				const auto path = directory / file_name;
				std::ifstream stream(path, std::ifstream::out);
				if (!stream.fail()) {
					InfoFileReader reader;
					if (reader.read(stream)) {
						const auto version_index = find_index(reader.getHeader(), "Version!STRING:0");
						const auto product_index = find_index(reader.getHeader(), "Product!STRING:0");
						const auto tags_index = find_index(reader.getHeader(), "Tags!STRING:0");

						if (version_index >= 0 && product_index >= 0 && tags_index >= 0) {
							for (const auto& row : reader.getRows()) {
								const auto ver = Utility::GameVersion::fromString(row[version_index]);
								if (ver.has_value()) {							
									ClientInfo info;
									info.name = row[product_index];
									info.locale = "";
									info.version = ver.value();

									std::smatch match;
									if (std::regex_search(row[tags_index], match, locale_regex)) {
										info.locale = match[1];
									}

									result.push_back(std::move(info));
								}
							}
						}
					}
				}

				if (result.size() > 0) {
					break;
				}
			}
			catch ([[maybe_unused]] std::exception& e) {
				// no action needed.
				result.clear();
			}
		}

		return result;
	}

    Detector::result_t Detector::filesystemHints(const std::filesystem::path& directory)
	{
		result_t result;

#ifdef WIN32

        auto GetPropertyStore = [](PCWSTR pszFilename, GETPROPERTYSTOREFLAGS gpsFlags, IPropertyStore** ppps) -> HRESULT
        {
            WCHAR szExpanded[MAX_PATH];
            HRESULT hr = ExpandEnvironmentStringsW(pszFilename, szExpanded, ARRAYSIZE(szExpanded)) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
            if (SUCCEEDED(hr))
            {
                WCHAR szAbsPath[MAX_PATH];
                hr = _wfullpath(szAbsPath, szExpanded, ARRAYSIZE(szAbsPath)) ? S_OK : E_FAIL;
                if (SUCCEEDED(hr))
                {
                    hr = SHGetPropertyStoreFromParsingName(szAbsPath, NULL, gpsFlags, IID_PPV_ARGS(ppps));
                }
            }
            return hr;
        };

		const auto data_directory = directory / "Data";
		const std::array<std::string, 2> exe_names = {
			"Wow.exe",
			"Wow-64.exe"
		};

		const LPCWCHAR attr_name = L"System.FileVersion";

		for (const auto& exe_name : exe_names) {
			const auto exe_path = directory / exe_name;
			if (std::filesystem::exists(exe_path)) {
				std::wstring wow_ver = L"";
				{
					PROPERTYKEY key;
					HRESULT hr = PSGetPropertyKeyFromName(attr_name, &key);
					if (SUCCEEDED(hr))
					{
						IPropertyStore* pps = NULL;
						hr = GetPropertyStore(exe_path.c_str(), GPS_DEFAULT, &pps);
						if (SUCCEEDED(hr))
						{
							PROPVARIANT propvarValue = { 0 };
							hr = pps->GetValue(key, &propvarValue);

							if (SUCCEEDED(hr))
							{
								PWSTR pszDisplayValue = NULL;
								hr = PSFormatForDisplayAlloc(key, propvarValue, PDFF_DEFAULT, &pszDisplayValue);
								if (SUCCEEDED(hr))
								{
									wow_ver.append(pszDisplayValue);
									CoTaskMemFree(pszDisplayValue);
								}
								PropVariantClear(&propvarValue);
							}

							pps->Release();
						}
					}
				}

				if (wow_ver.size() > 0) {
					const auto game_ver = Utility::GameVersion::fromString(std::string(wow_ver.begin(), wow_ver.end()));

					if (game_ver.has_value()) {

						ClientInfo info;
						info.name = "";
						info.version = game_ver.value();
						info.locale = "";

						const std::regex locale_regex{ "([a-z]{2}[A-Z]{2})" };

						for (const auto& item : std::filesystem::directory_iterator(data_directory)) {
							if (item.is_directory()) {
								const auto item_name = item.path().filename().string();
								std::smatch match;
								if (std::regex_search(item_name, match, locale_regex)) {
									info.locale = match[1];
									break;
								}
							}
						}

						result.push_back(std::move(info));
						break;
					}
				}
			}
		}
        
#endif

        return result;
    }

}