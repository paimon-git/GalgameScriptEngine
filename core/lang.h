#pragma once

#include <initializer_list>
#include <string>
#include <vector>

// 界面文案（Minecraft 风格的语言文件）
//
// 所有会显示给玩家看的文字都放在 assets/lang/<语言>.lang 里，用 key=value 编写；
// 代码里用 tr("key") 取文案。这样做的好处：
//   * 文案集中在一处，改文案/加语言不用翻代码
//   * 字体只按"语言文件里出现的字 + 剧本里的字"光栅化，新增文案不会再漏字变成问号
//   * key 写错时 tr() 会原样返回 key，界面上直接看得见，不会静默出错
namespace lang
{
// 载入语言文件；成功返回 true。重复载入会覆盖已有内容。
bool load(const std::string& path);

// 取文案；key 不存在时返回 key 本身（方便一眼看出漏了哪条）
const char* tr(const char* key);

// 带 %s 占位符的文案，按顺序替换
std::string trf(const char* key, std::initializer_list<std::string> args);

// 语言文件里的全部文案（供字体收集字形用）
std::vector<std::string> allValues();
}
