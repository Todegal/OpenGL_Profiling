#include "pch.h"
#include "CppUnitTest.h"

#include <string>
#include <iostream>
#include <filesystem>

namespace testFramework = Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

template<> inline std::wstring testFramework::ToString<std::filesystem::path>(const std::filesystem::path& p) { RETURN_WIDE_STRING(p); }

namespace tests
{

	TEST_CLASS(tests)
	{
	public:
		

		TEST_METHOD(TestMethod1)
		{
			const std::filesystem::path A = "/A/../A/B/C";
			const std::filesystem::path B = "/A/B/../B/C/../C/";
			Assert::AreEqual(std::filesystem::absolute(A), std::filesystem::absolute(B));
		}
	};
}
