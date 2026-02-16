/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Assert.h"

#include "Utility/CVar/CVar.h"
#include "Utility/CVar/CVarTypes.h"
#include "Utility/CVar/CVarMacros.h"
#include "Utility/CVar/IniParser.h"
#include "Utility/CVar/ConsoleCommand.h"

#include <fstream>
#include <thread>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>

DECLARE_LOG_CATEGORY(LogTest)

// Test CVars
AUTO_CVAR_BOOL(Test_Bool, true, "Test boolean CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_INT(Test_Int, 42, "Test integer CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(Test_Float, 3.14f, "Test float CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_STRING(Test_String, "Default", "Test string CVar", EConsoleVariableFlag::Saved)

// Reference CVars
static bool g_RefBool = false;
static int32_t g_RefInt = 100;
static float g_RefFloat = 2.71f;
static std::string g_RefString = "RefDefault";

AUTO_CVAR_REF_BOOL(Test_RefBool, g_RefBool, "Test reference boolean CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_REF_INT(Test_RefInt, g_RefInt, "Test reference integer CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_REF_FLOAT(Test_RefFloat, g_RefFloat, "Test reference float CVar", EConsoleVariableFlag::Saved)
AUTO_CVAR_REF_STRING(Test_RefString, g_RefString, "Test reference string CVar", EConsoleVariableFlag::Saved)

// Test-only CVars (not saved)
AUTO_CVAR_BOOL(Test_Only, false, "Test-only CVar", EConsoleVariableFlag::None)
AUTO_CVAR_INT(Test_ReadOnly, 999, "Read-only test CVar", EConsoleVariableFlag::ReadOnly)

RECORD(CVar_BasicFunctionality, true, 0, 1)
{
    SECTION(BoolCVarTest, true, 1,
        {
            // Test initial value
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == true, TXT("Bool CVar should be initialized to true"));
            
            // Test setting value
            CVar_Test_Bool.SetValue(false);
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("Bool CVar should be set to false"));
            
            // Test string conversion
            CVar_Test_Bool.SetValueFromString("true");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == true, TXT("Bool CVar should parse 'true' string"));
            
            CVar_Test_Bool.SetValueFromString("1");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == true, TXT("Bool CVar should parse '1' string"));
            
            CVar_Test_Bool.SetValueFromString("on");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == true, TXT("Bool CVar should parse 'on' string"));
            
            CVar_Test_Bool.SetValueFromString("false");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("Bool CVar should parse 'false' string"));
            
            CVar_Test_Bool.SetValueFromString("0");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("Bool CVar should parse '0' string"));
            
            CVar_Test_Bool.SetValueFromString("off");
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("Bool CVar should parse 'off' string"));
        });

    SECTION(IntCVarTest, true, 1,
        {
            // Test initial value
            HLVM_ENSURE_F(CVar_Test_Int.GetValue() == 42, TXT("Int CVar should be initialized to 42"));
            
            // Test setting value
            CVar_Test_Int.SetValue(100);
            HLVM_ENSURE_F(CVar_Test_Int.GetValue() == 100, TXT("Int CVar should be set to 100"));
            
            // Test string conversion
            CVar_Test_Int.SetValueFromString("42");
            HLVM_ENSURE_F(CVar_Test_Int.GetValue() == 42, TXT("Int CVar should parse '42' string"));
        });

    SECTION(FloatCVarTest, true, 1,
        {
            // Test initial value
            HLVM_ENSURE_F(std::abs(CVar_Test_Float.GetValue() - 3.14f) < 0.001f, TXT("Float CVar should be initialized to 3.14"));
            
            // Test setting value
            CVar_Test_Float.SetValue(2.71f);
            HLVM_ENSURE_F(std::abs(CVar_Test_Float.GetValue() - 2.71f) < 0.001f, TXT("Float CVar should be set to 2.71"));
            
            // Test string conversion
            CVar_Test_Float.SetValueFromString("1.618");
            HLVM_ENSURE_F(std::abs(CVar_Test_Float.GetValue() - 1.618f) < 0.001f, TXT("Float CVar should parse '1.618' string"));
        });

    SECTION(StringCVarTest, true, 1,
        {
            // Test initial value
            HLVM_ENSURE_F(CVar_Test_String.GetValue() == "Default", TXT("String CVar should be initialized to 'Default'"));
            
            // Test setting value
            CVar_Test_String.SetValue("Modified");
            HLVM_ENSURE_F(CVar_Test_String.GetValue() == "Modified", TXT("String CVar should be set to 'Modified'"));
            
            // Test string conversion
            CVar_Test_String.SetValueFromString("NewValue");
            HLVM_ENSURE_F(CVar_Test_String.GetValue() == "NewValue", TXT("String CVar should parse 'NewValue' string"));
        });

    SECTION(ReferenceCVarTest, true, 1,
        {
            // Test initial reference values
            HLVM_ENSURE_F(g_RefBool == false, TXT("Reference bool should be initialized to false"));
            HLVM_ENSURE_F(g_RefInt == 100, TXT("Reference int should be initialized to 100"));
            HLVM_ENSURE_F(std::abs(g_RefFloat - 2.71f) < 0.001f, TXT("Reference float should be initialized to 2.71"));
            HLVM_ENSURE_F(g_RefString == "RefDefault", TXT("Reference string should be initialized to 'RefDefault'"));
            
            // Test modifying through CVar
			CVarRef_Test_RefBool.SetValue(true);
            HLVM_ENSURE_F(g_RefBool == true, TXT("Reference bool should be updated through CVar"));
            
			CVarRef_Test_RefInt.SetValue(200);
            HLVM_ENSURE_F(g_RefInt == 200, TXT("Reference int should be updated through CVar"));
            
			CVarRef_Test_RefFloat.SetValue(3.14f);
            HLVM_ENSURE_F(std::abs(g_RefFloat - 3.14f) < 0.001f, TXT("Reference float should be updated through CVar"));
            
			CVarRef_Test_RefString.SetValue("Updated");
            HLVM_ENSURE_F(g_RefString == "Updated", TXT("Reference string should be updated through CVar"));
        });
}

RECORD(CVar_Manager, true, 0, 1)
{
    SECTION(ManagerRegistrationTest, true, 1,
        {
            CVarManager& manager = GetCVarManager();
            
            // Test finding registered CVars
            ICVar* boolCVar = manager.FindCVar("Test_Bool");
            HLVM_ENSURE_F(boolCVar != nullptr, TXT("Should find registered Bool CVar"));
            HLVM_ENSURE_F(boolCVar->GetName() == "Test_Bool", TXT("CVar name should match"));
            
            ICVar* intCVar = manager.FindCVar("Test_Int");
            HLVM_ENSURE_F(intCVar != nullptr, TXT("Should find registered Int CVar"));
            
            // Test non-existent CVar
            ICVar* nonExistent = manager.FindCVar("NonExistent");
            HLVM_ENSURE_F(nonExistent == nullptr, TXT("Should return nullptr for non-existent CVar"));
        });

    SECTION(ManagerValueOperationsTest, true, 1,
        {
            CVarManager& manager = GetCVarManager();

            // Test SetCVarValue
            bool success = manager.SetCVarValue("Test_Bool", "false");
            HLVM_ENSURE_F(success == true, TXT("SetCVarValue should succeed for existing CVar"));
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("CVar should be updated"));

            // Test GetCVarValue
            FString value = manager.GetCVarValue("Test_Int");
            HLVM_ENSURE_F(value == "42", TXT("GetCVarValue should return correct value"));

            // Test ResetCVar
            CVar_Test_Int.SetValue(999);
            manager.ResetCVar("Test_Int");
            HLVM_ENSURE_F(CVar_Test_Int.GetValue() == 42, TXT("ResetCVar should restore default value"));
        });

    SECTION(ManagerFlagsTest, true, 1,
        {
            CVarManager& manager = GetCVarManager();
            
            // Test ReadOnly flag
            bool success = manager.SetCVarValue("Test_ReadOnly", "123");
            HLVM_ENSURE_F(success == false, TXT("SetCVarValue should fail for ReadOnly CVar"));
            HLVM_ENSURE_F(CVar_Test_ReadOnly.GetValue() == 999, TXT("ReadOnly CVar should not change"));
        });

    SECTION(ManagerThreadSafetyTest, true, 1,
        {
			using namespace std::chrono_literals;
            CVarManager& manager = GetCVarManager();
            std::atomic<bool> testPassed{true};
            
            // Spawn multiple threads that modify CVars
            std::thread t1([&manager, &testPassed]() {
                for (int i = 0; i < 10; ++i) {
                    if (!manager.SetCVarValue("Test_Int", std::to_string(i))) {
                        testPassed = false;
                        break;
                    }
                    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
					boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
                }
            });
            
            std::thread t2([&manager, &testPassed]() {
                for (int i = 0; i < 10; ++i) {
                    FString value = manager.GetCVarValue("Test_Int");
                    if (value.empty()) {
                        testPassed = false;
                        break;
                    }
                    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
					boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
                }
            });
            
            t1.join();
            t2.join();
            
            HLVM_ENSURE_F(testPassed == true, TXT("Thread safety test should pass"));
        });
}

RECORD(IniParserTest, true, 0, 1)
{
    SECTION(IniParsingTest, true, 1,
        {
            // Create test ini file
            std::ofstream testFile("TestConfig.ini");
            testFile << "[System]\n";
            testFile << "TestValue1=123\n";
            testFile << "TestValue2=Hello World\n";
            testFile << "TestValue3=3.14\n";
            testFile.close();
            
            // Parse the file
            IniParser parser;
            bool success = parser.ParseFile("TestConfig.ini");
            HLVM_ENSURE_F(success == true, TXT("IniParser should successfully parse valid file"));
            
            // Check parsed values
            HLVM_ENSURE_F(parser.GetValue("System", "TestValue1") == "123", TXT("Should parse TestValue1 correctly"));
            HLVM_ENSURE_F(parser.GetValue("System", "TestValue2") == "Hello World", TXT("Should parse TestValue2 correctly"));
            HLVM_ENSURE_F(parser.GetValue("System", "TestValue3") == "3.14", TXT("Should parse TestValue3 correctly"));
            
            // Test default value
            HLVM_ENSURE_F(parser.GetValue("System", "NonExistent", "Default") == "Default", TXT("Should return default value for non-existent key"));
            
            // Clean up
            std::remove("TestConfig.ini");
        });

    SECTION(IniSavingTest, true, 1,
        {
            // Create parser and set some values
            IniParser parser;
            parser.SetValue("TestSection", "Key1", "Value1");
            parser.SetValue("TestSection", "Key2", "Value2");
            parser.SetValue("OtherSection", "Key3", "Value3");
            
            // Save to file
            bool success = parser.SaveToFile("TestOutput.ini");
            HLVM_ENSURE_F(success == true, TXT("Should successfully save ini file"));
            
            // Read back and verify
            std::ifstream file("TestOutput.ini");
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            
            HLVM_ENSURE_F(content.find("[TestSection]") != std::string::npos, TXT("Should contain TestSection"));
            HLVM_ENSURE_F(content.find("Key1=Value1") != std::string::npos, TXT("Should contain Key1=Value1"));
            HLVM_ENSURE_F(content.find("Key2=Value2") != std::string::npos, TXT("Should contain Key2=Value2"));
            HLVM_ENSURE_F(content.find("[OtherSection]") != std::string::npos, TXT("Should contain OtherSection"));
            HLVM_ENSURE_F(content.find("Key3=Value3") != std::string::npos, TXT("Should contain Key3=Value3"));
            
            // Clean up
            std::remove("TestOutput.ini");
        });
}

RECORD(ConsoleCommandTest, true, 0, 1)
{
    SECTION(ConsoleCommandExecutionTest, true, 1,
        {
            ConsoleCommandManager& cmdManager = ConsoleCommandManager::Get();
            
            // Test Set command
            bool success = cmdManager.ExecuteCommand("Set Test_Bool false");
            HLVM_ENSURE_F(success == true, TXT("Set command should succeed"));
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("CVar should be updated by Set command"));
            
            // Test Get command
            CVar_Test_Int.SetValue(777);
            success = cmdManager.ExecuteCommand("Get Test_Int");
            HLVM_ENSURE_F(success == true, TXT("Get command should succeed"));
            
            // Test Reset command
            CVar_Test_Float.SetValue(999.0f);
            success = cmdManager.ExecuteCommand("Reset Test_Float");
            HLVM_ENSURE_F(success == true, TXT("Reset command should succeed"));
            HLVM_ENSURE_F(std::abs(CVar_Test_Float.GetValue() - 3.14f) < 0.001f, TXT("CVar should be reset to default"));
            
            // Test Dump command
            success = cmdManager.ExecuteCommand("Dump Test_");
            HLVM_ENSURE_F(success == true, TXT("Dump command should succeed"));
            
            // Test Help command
            success = cmdManager.ExecuteCommand("Help Set");
            HLVM_ENSURE_F(success == true, TXT("Help command should succeed"));
            
            // Test invalid command
            success = cmdManager.ExecuteCommand("InvalidCommand");
            HLVM_ENSURE_F(success == false, TXT("Invalid command should fail"));
        });
}

RECORD(CVarPersistenceTest, true, 0, 1)
{
    SECTION(IniPersistenceTest, true, 1,
        {
            CVarManager& manager = GetCVarManager();
            
            // Modify some CVars with Saved flag
            CVar_Test_Bool.SetValue(false);
            CVar_Test_Int.SetValue(555);
            CVar_Test_Float.SetValue(1.23f);
            CVar_Test_String.SetValue("Persisted");
            
            // Save to ini
            bool success = manager.SaveToIni("TestPersistence.ini");
            HLVM_ENSURE_F(success == true, TXT("Should successfully save CVars to ini"));
            
            // Reset CVars
            CVar_Test_Bool.SetValue(true);
            CVar_Test_Int.SetValue(42);
            CVar_Test_Float.SetValue(3.14f);
            CVar_Test_String.SetValue("Default");
            
            // Load from ini
            success = manager.LoadFromIni("TestPersistence.ini");
            HLVM_ENSURE_F(success == true, TXT("Should successfully load CVars from ini"));
            
            // Verify values were restored
            HLVM_ENSURE_F(CVar_Test_Bool.GetValue() == false, TXT("Bool CVar should be restored from ini"));
            HLVM_ENSURE_F(CVar_Test_Int.GetValue() == 555, TXT("Int CVar should be restored from ini"));
            HLVM_ENSURE_F(std::abs(CVar_Test_Float.GetValue() - 1.23f) < 0.001f, TXT("Float CVar should be restored from ini"));
            HLVM_ENSURE_F(CVar_Test_String.GetValue() == "Persisted", TXT("String CVar should be restored from ini"));
            
            // Test that non-saved CVars are not persisted
            HLVM_ENSURE_F(CVar_Test_Only.GetValue() == false, TXT("Non-saved CVar should not be in ini"));
            
            // Clean up
            std::remove("TestPersistence.ini");
        });
}
