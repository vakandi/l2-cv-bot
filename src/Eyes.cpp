#include "Eyes.h"

#include <limits>
#include <algorithm>

void Eyes::Open(const cv::Mat &bgr)
{
    m_bgr = bgr.clone();
    
    // hide myself
    cv::circle(m_bgr, {m_bgr.cols / 2, m_bgr.rows / 2}, m_blind_spot_radius, 0, -1);
    cv::cvtColor(m_bgr, m_hsv, cv::COLOR_BGR2HSV);
}

std::vector<Eyes::NPC> Eyes::DetectNPCs()
{
    // extract regions with white NPC names
    cv::Mat white;
    cv::inRange(m_hsv, m_npc_name_color_from_hsv, m_npc_name_color_to_hsv, white);

    // increase white regions size
    cv::Mat mask;
    auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::dilate(white, mask, kernel);

    // join words
    kernel = cv::getStructuringElement(cv::MORPH_RECT, {17, 5});
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // remove noise
    kernel = cv::getStructuringElement(cv::MORPH_RECT, {11, 5});
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<NPC> npcs;

    for (const auto &contour : contours) {
        const auto rect = cv::boundingRect(contour);

        if (rect.height < m_npc_name_min_height || rect.height > m_npc_name_max_height ||
            rect.width < m_npc_name_min_width || rect.width > m_npc_name_max_width ||
            rect.width < rect.height * 2
        ) {
            continue;
        }

        const auto target_image = white(rect);
        const auto threshold = cv::countNonZero(target_image) / target_image.total();

        if (threshold > m_npc_name_color_threshold) {
            continue;
        }

        NPC npc = {};
        npc.rect = rect;
        npc.center = {rect.x + rect.width / 2, rect.y + rect.height / 2 + m_npc_name_center_offset};
        npc.name_id = Hash(target_image);
        npc.state = DetectNPCState(rect);
        npcs.push_back(npc);
    }
    
    CalculateTrackingIds(npcs);

    m_npcs = npcs;
    return npcs;
}

std::vector<Eyes::FarNPC> Eyes::DetectFarNPCs()
{
    if (m_far_npc_limit <= 0) {
        return {};
    }
    
    // diff current frame with 3 previous frames
    cv::Mat diff_sum;

    for (decltype(m_frame) i = m_frame; i-- > m_frame - 3;) {
        const auto frame = m_hsv_frames[i % m_hsv_frames.size()];

        if (frame.empty()) {
            continue;
        }

        cv::Mat diff;
        cv::absdiff(frame, m_hsv, diff);

        if (diff_sum.empty()) {
            diff_sum = diff;
        } else {
            cv::bitwise_or(diff, diff_sum, diff_sum);
        }
    }

    // expand diff areas
    if (!diff_sum.empty()) {
        cv::cvtColor(diff_sum, diff_sum, cv::COLOR_BGR2GRAY);
        cv::threshold(diff_sum, diff_sum, 5, 255, cv::THRESH_BINARY);
        const auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {21, 21});
        cv::dilate(diff_sum, diff_sum, kernel);
    }

    // multiply all diffs
    cv::Mat mask;

    for (const auto &diff : m_diffs) {
        if (diff.empty()) {
            continue;
        }

        if (mask.empty()) {
            mask = diff;
        } else {
            cv::bitwise_and(diff, mask, mask);
        }
    }

    m_diffs[m_frame % m_diffs.size()] = diff_sum;

    if (mask.empty()) {
        return {};
    }

    // remove noise
    const auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {15, 15});
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<FarNPC> npcs;

    for (const auto &contour : contours) {
        const auto rect = cv::boundingRect(contour);

        if (rect.height < m_far_npc_min_height || rect.height > m_far_npc_max_height ||
            rect.width < m_far_npc_min_width || rect.width > m_far_npc_max_width ||
            rect.y + rect.height > m_hsv.rows / 2
        ) {
            continue;
        }

        FarNPC npc = {};
        npc.rect = rect;
        npc.center = {npc.rect.x + npc.rect.width / 2, npc.rect.y + npc.rect.height / 2};
        npcs.push_back(npc);
    }

    // return only nearest NPCs
    std::sort(npcs.begin(), npcs.end(), [this](const FarNPC &a, const FarNPC &b) {
        return (std::min)(a.rect.width, a.rect.height) > (std::min)(b.rect.width, b.rect.height);
    });

    if (npcs.size() > m_far_npc_limit) {
        npcs.erase(npcs.begin() + m_far_npc_limit, npcs.end());
    }

    CalculateTrackingIds(npcs);

    m_far_npcs = npcs;
    return npcs;
}

std::optional<Eyes::Me> Eyes::DetectMe()
{
    if (!m_my_bars.has_value()) {
        m_my_bars = DetectMyBars();
    }

    if (!m_my_bars.has_value()) {
        return {};
    }

    Me me = {};
    me.hp = CalcBarPercentValue(m_hsv(m_my_bars.value().hp_bar), m_my_hp_color_from_hsv, m_my_hp_color_to_hsv);
    me.mp = CalcBarPercentValue(m_hsv(m_my_bars.value().mp_bar), m_my_mp_color_from_hsv, m_my_mp_color_to_hsv);
    me.cp = CalcBarPercentValue(m_hsv(m_my_bars.value().cp_bar), m_my_cp_color_from_hsv, m_my_cp_color_to_hsv);
    return me;
}

std::optional<Eyes::Target> Eyes::DetectTarget()
{
    if (!m_target_hp_bar.has_value()) {
        m_target_hp_bar = DetectTargetHPBar();
    }

    if (!m_target_hp_bar.has_value()) {
        return {};
    }

    Target target = {};

    // Build inset ROI to avoid borders and focus on the bar's filled strip
    const auto bar_hsv_full = m_hsv(m_target_hp_bar.value());
    int inset_x = bar_hsv_full.cols > 2 ? 1 : 0;
    int roi_w   = (std::max)(1, bar_hsv_full.cols - inset_x * 2);
    int roi_y   = bar_hsv_full.rows / 3; // center strip
    int roi_h   = (std::max)(1, bar_hsv_full.rows / 3);
    if (roi_y + roi_h > bar_hsv_full.rows) {
        roi_y = (std::max)(0, bar_hsv_full.rows - roi_h);
    }
    cv::Rect roi_local{inset_x, roi_y, roi_w, roi_h};
    cv::Mat roiHsv = bar_hsv_full(roi_local).clone();

    // Auto-calibrate hue band periodically or on first use
    if (!m_target_hp_calibrated || (m_frame % m_target_hp_calibrate_every) == 0) {
        CalibrateTargetHpColor(roiHsv);
    }

    // Build mask and compute percentages
    cv::Mat hpMask = MakeHpMask(roiHsv);
    const int p_ratio = ComputeHpPercentFromMask(hpMask, roi_local);
    const int p_run   = ComputeHpPercentRunLength(hpMask);
    int p = (std::max)(p_ratio, p_run);

    // Smooth result with EMA
    m_target_hp_ema = (m_target_hp_ema < 0.0) ? p : (0.7 * m_target_hp_ema + 0.3 * p);
    target.hp = (int)(std::round((std::min)(100.0, (std::max)(0.0, m_target_hp_ema))));

        // Debug output every 30 frames
        static int debug_frame = 0;
        if (++debug_frame % 30 == 0) {
            std::cout << "Target HP Bar detected: " << m_target_hp_bar.value().width << "x" << m_target_hp_bar.value().height
                      << " at (" << m_target_hp_bar.value().x << "," << m_target_hp_bar.value().y << ") - HP: " << target.hp
                      << "%  hueCenter=" << m_target_hp_hue_center << ", span=" << m_target_hp_hue_span
                      << ", S>=" << m_target_hp_min_s << ", V>=" << m_target_hp_min_v
                      << ", p_ratio=" << p_ratio << ", p_run=" << p_run << std::endl;
            
            // Debug: show what colors we're actually seeing in the detected area
            if (m_target_hp_bar.has_value()) {
                const auto& bar_rect = m_target_hp_bar.value();
                cv::Mat bar_region = m_hsv(bar_rect);
                cv::Mat bar_bgr;
                cv::cvtColor(bar_region, bar_bgr, cv::COLOR_HSV2BGR);
                
                // Sample a few pixels from the detected bar area
                for (int y = 0; y < std::min(3, bar_bgr.rows); y++) {
                    for (int x = 0; x < std::min(5, bar_bgr.cols); x++) {
                        const uchar* pixel = bar_bgr.ptr<uchar>(y) + x * 3;
                        int b = pixel[0], g = pixel[1], r = pixel[2];
                        std::cout << "  Pixel(" << x << "," << y << "): BGR(" << (int)b << "," << (int)g << "," << (int)r << ")";
                        
                        // Check if this pixel would be detected as magenta
                        bool is_magenta = (r >= 180 && r <= 250 && g >= 0 && g <= 30 && b >= 180 && b <= 250 && r > g && b > g);
                        std::cout << " -> " << (is_magenta ? "MAGENTA" : "NOT MAGENTA") << std::endl;
                    }
                }
            }
        }

    return target;
}

std::optional<struct Eyes::MyBars> Eyes::DetectMyBars() const
{
    // extract red regions with red HP bar
    cv::Mat mask;
    cv::inRange(m_hsv, m_my_hp_color_from_hsv, m_my_hp_color_to_hsv, mask);

    const auto contours = FindMyBarContours(mask);

    // search for CP bar above and MP bar below
    for (const auto &contour : contours) {
        const auto rect = cv::boundingRect(contour);

        if (rect.height < m_my_bar_min_height || rect.height > m_my_bar_max_height ||
            rect.width < m_my_bar_min_width || rect.width > m_my_bar_max_width
        ) {
            continue;
        }

        // expand rect
        const auto bars_rect = rect + cv::Size{0, rect.height * 4} + cv::Point{0, -rect.height * 2};

        if (!IsRectInImage(m_hsv, bars_rect)) {
            continue;
        }

        const auto bars = m_hsv(bars_rect);

        // extract blue & yellow regions
        cv::Mat mp;
        cv::inRange(bars, m_my_mp_color_from_hsv, m_my_mp_color_to_hsv, mp);
        cv::Mat cp;
        cv::inRange(bars, m_my_cp_color_from_hsv, m_my_cp_color_to_hsv, cp);
        cv::Mat mp_cp;
        cv::bitwise_or(cp, mp, mp_cp);

        const auto bar_contours = FindMyBarContours(mp_cp);

        // no CP nor MP bar were found
        if (bar_contours.size() != 2) {
            continue;
        }

        struct MyBars my_bars = {};
        my_bars.hp_bar = rect;
        my_bars.mp_bar = cv::boundingRect(bar_contours[0]) + bars_rect.tl();
        my_bars.cp_bar = cv::boundingRect(bar_contours[1]) + bars_rect.tl();
        return my_bars;
    }

    return {};
}

std::optional<cv::Rect> Eyes::DetectTargetHPBar() const
{
    // Simple approach: just detect the red/pink HP bar color
    cv::Mat mask;
    cv::inRange(m_hsv, m_target_hp_color_from_hsv, m_target_hp_color_to_hsv, mask);
    
    // Remove noise with smaller kernel for better detection
    const auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {10, 2});
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Find the largest contour that matches our size criteria
    cv::Rect best_rect;
    int best_area = 0;
    
    for (const auto &contour : contours) {
        const auto rect = cv::boundingRect(contour);
        int area = rect.width * rect.height;

        if (rect.height >= m_target_hp_min_height && rect.height <= m_target_hp_max_height &&
            rect.width >= m_target_hp_min_width && rect.width <= m_target_hp_max_width &&
            area > best_area) {
            best_rect = rect;
            best_area = area;
        }
    }
    
    if (best_area > 0) {
        return best_rect;
    }

    return {};
}

std::vector<std::vector<cv::Point>> Eyes::FindMyBarContours(const cv::Mat &mask) const
{
    // remove noise
    auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {1, m_my_bar_min_height});
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);

    // join parts of the bar
    kernel = cv::getStructuringElement(cv::MORPH_RECT, {25, 1});
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

Eyes::NPC::State Eyes::DetectNPCState(const cv::Rect &rect) const
{
    // expand rect
    const auto expanded_rect = rect +
        cv::Size(m_target_circle_area_width * 2, m_target_circle_area_height - rect.height) +
        cv::Point(-m_target_circle_area_width, -(m_target_circle_area_height - rect.height) / 2);
    
    if (!IsRectInImage(m_hsv, expanded_rect)) {
        return NPC::State::Default;
    }

    cv::Mat bgr = m_bgr(expanded_rect);
    cv::rectangle(bgr, {m_target_circle_area_width, 0, rect.width, m_target_circle_area_height}, 0, -1);

    // extract circles
    cv::Mat gray;
    cv::inRange(bgr, m_target_gray_circle_color_bgr, m_target_gray_circle_color_bgr, gray);
    cv::Mat blue;
    cv::inRange(bgr, m_target_blue_circle_color_bgr, m_target_blue_circle_color_bgr, blue);
    cv::Mat red;
    cv::inRange(bgr, m_target_red_circle_color_bgr, m_target_red_circle_color_bgr, red);
    cv::Mat mask;
    cv::bitwise_or(gray, blue, mask);
    cv::bitwise_or(mask, red, mask);

    // increase regions size
    const auto kernel = cv::getStructuringElement(cv::MORPH_RECT, {5, 5});
    cv::dilate(mask, mask, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.size() < 2) {
        return NPC::State::Default;
    }

    bool gray_found = cv::countNonZero(gray) > 0;

    // compare each contour to find pair
    for (const auto &contour1 : contours) {
        for (const auto &contour2 : contours) {
            if (contour1 == contour2) {
                continue;
            }

            const auto rect1 = cv::boundingRect(contour1);
            const auto rect2 = cv::boundingRect(contour2);

            if (rect1.y != rect2.y || rect1.size() != rect2.size()) {
                continue;
            }

            return gray_found ? NPC::State::Hovered : NPC::State::Selected;
        }
    }

    return NPC::State::Default;
}

template<typename T>
void Eyes::CalculateTrackingIds(std::vector<T> &npcs) const
{
    std::uint32_t max_tracking_id = 0;

    for (auto &npc : npcs) {
        npc.tracking_id = 0;

        for (const auto &previous_npc : m_npcs) {
            const auto distance = std::hypot(previous_npc.center.x - npc.center.x, previous_npc.center.y - npc.center.y);

            if (distance <= m_npc_tracking_distance) {
                npc.tracking_id = previous_npc.tracking_id;
                max_tracking_id = (std::max)(max_tracking_id, previous_npc.tracking_id);
                break;
            }
        }
    }

    std::uint32_t tracking_id = max_tracking_id;

    for (auto &npc : npcs) {
        if (npc.tracking_id == 0) {
            npc.tracking_id = ++tracking_id;
        }
    }
}

int Eyes::CalcBarPercentValue(
    const cv::Mat &bar,
    const cv::Scalar &from_color,
    const cv::Scalar &to_color,
    bool whole_bar
) {
    CV_Assert(bar.rows >= 1);
    CV_Assert(bar.depth() == CV_8U);
    CV_Assert(bar.channels() >= 3);

    if (whole_bar) {
        // For target HP bars: detect red/pink pixels based on actual photo analysis
        int total_pixels = bar.rows * bar.cols;
        int red_pixels = 0;
        
        // Debug: sample a few pixels to see what colors we're getting
        static int debug_count = 0;
        if (++debug_count % 100 == 0 && total_pixels > 0) {
            const uchar* sample_row = bar.ptr<uchar>(bar.rows / 2);
            int sample_x = bar.cols / 2;
            int b = sample_row[sample_x * 3 + 0];
            int g = sample_row[sample_x * 3 + 1]; 
            int r = sample_row[sample_x * 3 + 2];
            std::cout << "Sample pixel at center: B=" << (int)b << " G=" << (int)g << " R=" << (int)r << std::endl;
        }
        
        // Count pixels that match the red/pink color range
        // Based on photo analysis: Red pixels have R=111-171, G=23-48, B=19-34
        for (int y = 0; y < bar.rows; y++) {
            const uchar* row = bar.ptr<uchar>(y);
            for (int x = 0; x < bar.cols; x++) {
                int b = row[x * 3 + 0];  // Blue
                int g = row[x * 3 + 1];  // Green  
                int r = row[x * 3 + 2];  // Red
                
                // Detect magenta/purple HP bar pixels based on actual game colors
                // From game logs: BGR(219,2,235), BGR(124,16,187), BGR(128,26,207)
                // These are magenta/purple colors, not red!
                bool is_magenta = (r >= 180 && r <= 250 &&    // Red channel: very high (magenta)
                                  g >= 0 && g <= 30 &&        // Green channel: very low
                                  b >= 180 && b <= 250 &&     // Blue channel: very high (magenta)
                                  r > g && b > g);            // Red and Blue dominate Green
                              
                if (is_magenta) {
                    red_pixels++;
                }
            }
        }
        
        // Calculate percentage: red pixels / total pixels
        if (total_pixels > 0) {
            int percentage = (red_pixels * 100) / total_pixels;
            if (debug_count % 100 == 0) {
                std::cout << "Red pixels: " << red_pixels << "/" << total_pixels << " = " << percentage << "%" << std::endl;
            }
            return percentage;
        }
        return 0;
    } else {
        // Original algorithm for my HP/MP/CP bars (working perfectly)
        const auto row = bar.ptr<uchar>(bar.rows / 2);
        auto channel = (bar.cols - 1) * bar.channels();
        auto cols = bar.cols;

        for (; channel > 0; channel -= bar.channels()) {
            if (row[channel + 0] >= from_color[0] && row[channel + 0] <= to_color[0] &&
                row[channel + 1] >= from_color[1] && row[channel + 1] <= to_color[1] &&
                row[channel + 2] >= from_color[2] && row[channel + 2] <= to_color[2]
            ) {
                break;
            } else {
                cols--;
            }
        }
        return cols * 100 / bar.cols;
    }
}

std::uint32_t Eyes::Hash(const cv::Mat &image)
{
    // djb2 hash
    std::uint32_t hash = 5381;
    const auto total = image.total();

    for (std::size_t i = 0; i < total; ++i) {
        hash = ((hash << 5) + hash) ^ *(image.data + i);
    }

    return hash;
}

void Eyes::CalibrateTargetHpColor(const cv::Mat &roiHsv)
{
    CV_Assert(!roiHsv.empty());
    std::array<int, 180> hist{};
    int considered = 0;

    for (int y = 0; y < roiHsv.rows; ++y) {
        const auto* row = roiHsv.ptr<cv::Vec3b>(y);
        for (int x = 0; x < roiHsv.cols; ++x) {
            const auto H = (int)row[x][0];
            const auto S = (int)row[x][1];
            const auto V = (int)row[x][2];
            if (S >= m_target_hp_min_s && V >= m_target_hp_min_v) {
                hist[H]++;
                considered++;
            }
        }
    }

    const int total = roiHsv.rows * roiHsv.cols;
    if (considered < (total * 5) / 100) {
        // Fallback to magenta band if not enough saturated/bright pixels
        m_target_hp_hue_center = 157; // middle of [140,175]
        m_target_hp_hue_span   = 18;  // ~[139,175]
        m_target_hp_calibrated = true;
        return;
    }

    const int center = (int)std::distance(hist.begin(), std::max_element(hist.begin(), hist.end()));
    m_target_hp_hue_center = center;
    m_target_hp_calibrated = true;
}

cv::Mat Eyes::MakeHpMask(const cv::Mat &roiHsv) const
{
    CV_Assert(!roiHsv.empty());
    // S/V gating
    cv::Mat gateS, gateV, gateSV;
    cv::inRange(roiHsv, cv::Scalar(0, m_target_hp_min_s, 0), cv::Scalar(179, 255, 255), gateS);
    cv::inRange(roiHsv, cv::Scalar(0, 0, m_target_hp_min_v), cv::Scalar(179, 255, 255), gateV);
    cv::bitwise_and(gateS, gateV, gateSV);

    // Hue band(s) with wrap-around handling
    int span = m_target_hp_hue_span;
    int a = m_target_hp_hue_center - span;
    int b = m_target_hp_hue_center + span;
    cv::Mat m1, m2;
    if (a < 0) {
        cv::inRange(roiHsv, cv::Scalar(0, 0, 0), cv::Scalar(b, 255, 255), m1);
        cv::inRange(roiHsv, cv::Scalar(180 + a, 0, 0), cv::Scalar(179, 255, 255), m2);
    } else if (b > 179) {
        cv::inRange(roiHsv, cv::Scalar(a, 0, 0), cv::Scalar(179, 255, 255), m1);
        cv::inRange(roiHsv, cv::Scalar(0, 0, 0), cv::Scalar(b - 180, 255, 255), m2);
    } else {
        cv::inRange(roiHsv, cv::Scalar(a, 0, 0), cv::Scalar(b, 255, 255), m1);
        m2 = cv::Mat::zeros(roiHsv.size(), CV_8U);
    }

    cv::Mat hueMask;
    cv::bitwise_or(m1, m2, hueMask);

    cv::Mat mask;
    cv::bitwise_and(hueMask, gateSV, mask);

    // Clean up noise slightly
    const auto k = cv::getStructuringElement(cv::MORPH_RECT, {3, 1});
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, k);
    return mask;
}

int Eyes::ComputeHpPercentFromMask(const cv::Mat &mask, const cv::Rect &roi) const
{
    const int total = roi.width * roi.height;
    if (total <= 0) return 0;
    const int filled = cv::countNonZero(mask);
    return (filled * 100) / total;
}

int Eyes::ComputeHpPercentRunLength(const cv::Mat &mask) const
{
    if (mask.empty()) return 0;
    const int mid = mask.rows / 2;
    int best = 0, cur = 0;
    for (int x = 0; x < mask.cols; ++x) {
        const int colNonZero = cv::countNonZero(mask.col(x));
        const bool filled = colNonZero * 2 >= mask.rows; // >=50% of column
        if (filled) {
            cur++;
            best = (std::max)(best, cur);
        } else {
            cur = 0;
        }
    }
    if (mask.cols <= 0) return 0;
    return (best * 100) / mask.cols;
}
