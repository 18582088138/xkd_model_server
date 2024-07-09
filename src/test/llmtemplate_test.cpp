//*****************************************************************************
// Copyright 2024 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************
#include <fstream>
#include <memory>
#include <string>

#include <continuous_batching_pipeline.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openvino/openvino.hpp>
#include <pybind11/embed.h>

#include "../llm/http_payload.hpp"
#include "../llm/llm_executor.hpp"
#include "../llm/llmnoderesources.hpp"
#include "../mediapipe_internal/mediapipegraphdefinition.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/calculator_runner.h"
#pragma GCC diagnostic pop

#include "test_utils.hpp"

using namespace ovms;

class LLMChatTemplateTest : public TestWithTempDir {
private:
    bool CreateConfig(std::string& fileContents, std::string& filePath) {
        std::ofstream configFile{filePath};
        if (!configFile.is_open()) {
            std::cout << "Failed to open " << filePath << std::endl;
            return false;
        }
        SPDLOG_INFO("Creating config file: {}\n with content:\n{}", filePath, fileContents);
        configFile << fileContents << std::endl;
        configFile.close();
        if (configFile.fail()) {
            SPDLOG_INFO("Closing configFile failed");
            return false;
        } else {
            SPDLOG_INFO("Closing configFile succeed");
        }

        return true;
    }

protected:
    std::string tokenizerConfigFilePath;
    std::string jinjaConfigFilePath;
    void SetUp() {
        py::initialize_interpreter();
        TestWithTempDir::SetUp();
        tokenizerConfigFilePath = directoryPath + "/tokenizer_config.json";
        jinjaConfigFilePath = directoryPath + "/template.jinja";
    }
    void TearDown() { py::finalize_interpreter(); }

public:
    bool CreateTokenizerConfig(std::string& fileContents) {
        return CreateConfig(fileContents, tokenizerConfigFilePath);
    }
    bool CreateJinjaConfig(std::string& fileContents) {
        return CreateConfig(fileContents, jinjaConfigFilePath);
    }
};

TEST_F(LLMChatTemplateTest, ChatTemplateEmptyBody) {
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    // default_chat_template = "{% if messages|length > 1 %} {{ raise_exception('This servable accepts only single message requests') }}{% endif %}{{ messages[0]['content'] }}"
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = "";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    std::string errorOutput = "Expecting value: line 1 column 1 (char 0)";
    ASSERT_EQ(finalPrompt, errorOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateEmptyMessage) {
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    // default_chat_template = "{% if messages|length > 1 %} {{ raise_exception('This servable accepts only single message requests') }}{% endif %}{{ messages[0]['content'] }}"
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": []
        }
    )";
    std::string errorOutput = "list object has no element 0";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, errorOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateDefault) {
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    // default_chat_template = "{% if messages|length > 1 %} {{ raise_exception('This servable accepts only single message requests') }}{% endif %}{{ messages[0]['content'] }}"
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "messages": [{ "content": "How can I help you?" }]
        }
    )";
    std::string expectedOutput = "How can I help you?";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateMultiMessage) {
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    // default_chat_template = "{% if messages|length > 1 %} {{ raise_exception('This servable accepts only single message requests') }}{% endif %}{{ messages[0]['content'] }}"
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "messages": [{ "content": "How can I help you?" }, { "content": "2How can I help you?" }]
        }
    )";
    std::string errorOutput = "This servable accepts only single message requests";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, errorOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateComplexMessage) {
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    // default_chat_template = "{% if messages|length > 1 %} {{ raise_exception('This servable accepts only single message requests') }}{% endif %}{{ messages[0]['content'] }}"
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = "hello";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateJinjaUppercase) {
    std::string jinjaTemplate = R"( {{ "Hi, " + messages[0]['content'] | upper }} )";
    ASSERT_EQ(CreateJinjaConfig(jinjaTemplate), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = " Hi, HELLO ";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateJinjaException) {
    std::string jinjaTemplate = R"( {{ "Hi, " + messages[3]['content'] | upper }} )";
    ASSERT_EQ(CreateJinjaConfig(jinjaTemplate), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string errorOutput = "list object has no element 3";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, errorOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTokenizerDefault) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>"
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = "hello";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTokenizerException) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>",
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = "Error: Chat template not loaded correctly, so it cannot be applied";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTokenizerUpperCase) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>",
    "chat_template": "{{ \"Hi, \" + messages[0]['content'] | upper }}"
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = "Hi, HELLO";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTokenizerTemplateException) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>",
    "chat_template": "{{ \"Hi, \" + messages[3]['content'] | upper }}"
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = "list object has no element 3";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, expectedOutput);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTokenizerTemplateBadVariable) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>",
    "chat_template": {}
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedError = "Error: Chat template not loaded correctly, so it cannot be applied";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), false);
    ASSERT_EQ(finalPrompt, expectedError);
}

TEST_F(LLMChatTemplateTest, ChatTemplateTwoConfigs) {
    std::string tokenizerJson = R"({
    "bos_token": "</s>",
    "eos_token": "</s>",
    "chat_template": "{{ \"Hi, \" + messages[0]['content'] | lower }}"
    })";
    ASSERT_EQ(CreateTokenizerConfig(tokenizerJson), true);
    std::string jinjaTemplate = R"( {{ "Hi, " + messages[0]['content'] | upper }} )";
    ASSERT_EQ(CreateJinjaConfig(jinjaTemplate), true);

    std::shared_ptr<LLMNodeResources> nodeResources = std::make_shared<LLMNodeResources>();
    nodeResources->modelsPath = directoryPath;
    LLMNodeResources::loadTextProcessor(nodeResources, nodeResources->modelsPath);

    std::string finalPrompt = "";
    std::string payloadBody = R"(
        {
            "model": "gpt",
            "stream": false,
            "messages": [{"role": "user", "content": "hello"}]
        }
    )";
    std::string expectedOutput = " Hi, HELLO ";
    ASSERT_EQ(applyChatTemplate(nodeResources->textProcessor, nodeResources->modelsPath, payloadBody, finalPrompt), true);
    ASSERT_EQ(finalPrompt, expectedOutput);
}