#pragma once

#include <memory>
#include <functional>

//#include <opencv2\opencv.hpp>


namespace TesseractWrapper {

bool	TesseractInit();
void	TesseractTerm();

//using TextFromImageFunc = std::function<std::wstring(cv::Mat)>;

//std::shared_ptr<TextFromImageFunc> GetOCRFunction();


std::wstring TextFromImage(const std::wstring& targetImagePath);

//std::wstring TextFromImageBest(cv::Mat targetImage);

}	// namespace TesseractWrapper

