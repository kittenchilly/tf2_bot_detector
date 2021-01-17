#include "tf2_bot_detector_winrt.h"
#include "tf2_bot_detector_winrt_export.h"

#include <Platform/PlatformCommon.h>

#include <mh/error/exception_details.hpp>
#include <mh/text/codecvt.hpp>

#include <winrt/Windows.Storage.h>
#include <Windows.h>
#include <minappmodel.h>

namespace
{
	class WinRTImpl final : public tf2_bot_detector::WinRT
	{
	public:
		WinRTImpl(const tf2_bot_detector::WinRT* fallback) : m_Fallback(fallback) {}

		std::filesystem::path GetLocalAppDataDir() const override;
		std::filesystem::path GetRoamingAppDataDir() const override;
		std::filesystem::path GetTempDir() const override;
		std::wstring GetCurrentPackageFamilyName() const override;

		const mh::exception_details_handler& GetWinRTExceptionDetailsHandler() const override;

	private:
		const tf2_bot_detector::WinRT* m_Fallback{};
	};

	std::filesystem::path WinRTImpl::GetLocalAppDataDir() const try
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.LocalFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}
	catch (...)
	{
		if (m_Fallback)
			return m_Fallback->GetLocalAppDataDir();

		throw;
	}

	std::filesystem::path WinRTImpl::GetRoamingAppDataDir() const try
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.RoamingFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}
	catch (...)
	{
		if (m_Fallback)
			return m_Fallback->GetRoamingAppDataDir();

		throw;
	}

	std::filesystem::path WinRTImpl::GetTempDir() const try
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.TemporaryFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}
	catch (...)
	{
		if (m_Fallback)
			return m_Fallback->GetTempDir();

		throw;
	}

	std::wstring WinRTImpl::GetCurrentPackageFamilyName() const try
	{
		static const std::wstring s_CurrentPackageFamilyName = []() -> std::wstring
		{
			WCHAR name[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
			UINT32 nameLength = UINT32(std::size(name));

			using func_type = LONG(*)(UINT32* packageFamilyNameLength, PWSTR packageFamilyName);

			const auto func = reinterpret_cast<func_type>(tf2_bot_detector::Platform::GetProcAddressHelper(
				"Kernel32.dll", "GetCurrentPackageFamilyName", true));

			const auto errc = func(&nameLength, name);

			switch (errc)
			{
			case ERROR_SUCCESS:
				return std::wstring(name, nameLength > 0 ? (nameLength - 1) : 0);
			case APPMODEL_ERROR_NO_PACKAGE:
				return {};
			case ERROR_INSUFFICIENT_BUFFER:
				throw std::runtime_error(mh::format("{}: Buffer too small", __FUNCTION__));
			default:
				throw std::runtime_error(mh::format("{}: Unknown error {}", __FUNCTION__, errc));
			}
		}();

		return s_CurrentPackageFamilyName;
	}
	catch (...)
	{
		if (m_Fallback)
			return m_Fallback->GetCurrentPackageFamilyName();

		throw;
	}

	const mh::exception_details_handler& WinRTImpl::GetWinRTExceptionDetailsHandler() const try
	{
		class Handler final : public mh::exception_details_handler
		{
		public:
			bool try_handle(const std::exception_ptr& e, mh::exception_details& details) const noexcept override
			{
				const auto FormatHRMessage = [](const winrt::hresult_error& hr)
				{
					return mh::format(MH_FMT_STRING("{:#x}: {}"),
						hr.code(), mh::change_encoding<char>(hr.message().c_str()));
				};

#define HR_ERR_HELPER(type) \
	catch (const type& e) \
	{ \
		details.m_Type = &typeid(e); \
		details.m_Message = FormatHRMessage(e); \
		return true; \
	}

				try
				{
					std::rethrow_exception(e);
				}
				HR_ERR_HELPER(winrt::hresult_access_denied)
				HR_ERR_HELPER(winrt::hresult_wrong_thread)
				HR_ERR_HELPER(winrt::hresult_not_implemented)
				HR_ERR_HELPER(winrt::hresult_invalid_argument)
				HR_ERR_HELPER(winrt::hresult_out_of_bounds)
				HR_ERR_HELPER(winrt::hresult_no_interface)
				HR_ERR_HELPER(winrt::hresult_class_not_available)
				HR_ERR_HELPER(winrt::hresult_class_not_registered)
				HR_ERR_HELPER(winrt::hresult_changed_state)
				HR_ERR_HELPER(winrt::hresult_illegal_method_call)
				HR_ERR_HELPER(winrt::hresult_illegal_state_change)
				HR_ERR_HELPER(winrt::hresult_illegal_delegate_assignment)
				HR_ERR_HELPER(winrt::hresult_canceled)
				HR_ERR_HELPER(winrt::hresult_error)

#undef HR_ERR_HELPER

				return false;
			}
		} static const s_Handler;

		return s_Handler;
	}
	catch (...)
	{
		if (m_Fallback)
			return m_Fallback->GetWinRTExceptionDetailsHandler();

		throw;
	}
}

extern "C" TF2_BOT_DETECTOR_WINRT_EXPORT tf2_bot_detector::WinRT* CreateWinRTInterface(const tf2_bot_detector::WinRT* fallback)
{
	return new WinRTImpl(fallback);
}