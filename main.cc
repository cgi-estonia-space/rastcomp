#include <iostream>
#include <vector>
#include "gdal/gdal_priv.h"
#include <chrono>
#include <algorithm>

#include <unistd.h>

void print_gdal_cache()
{
    double max = GDALGetCacheMax64();
    double current = GDALGetCacheUsed64();
    printf("gdal cache - %f GB , %f GB, %f %%\n", current/1e9, max/1e9, (current/max) * 100);
}


struct PixelData
{
    int16_t x;
    int16_t y;
    float pixel_1;
    float pixel_2;
    float diff;
    float rel_diff;

    void print()
    {
        printf("(%6d , %5d) pixel: [%.20f - %.20f] diff: %.20f ",
               x, y, pixel_1, pixel_2, diff);

        if(abs(rel_diff) > 0.001)
        {
            printf(" - %.6f%%\n", rel_diff * 100);
        }
        else
        {
            printf(" - %.6f ppm\n", rel_diff * 1e6);
        }

        uint32_t p1;
        uint32_t p2;
        memcpy(&p1, &pixel_1, 4);
        memcpy(&p2, &pixel_2, 4);
        printf("%08X %08X\n", p1, p2);
    }
};


int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("2 arguments required <golden> <comparison>");
        return 1;
    }

    GDALSetCacheMax64(0.1e9);
    GDALAllRegister();


    int BAND_NR1 = 1;
    int BAND_NR2 = 1;

    const char* file_1 = argv[1];
    const char* file_2 = argv[2];

    printf("f1 = %s\nf2 = %s\n, band = %d, %d\n", file_1, file_2, BAND_NR1, BAND_NR2);


    auto ds1 = (GDALDataset *) GDALOpen(file_1, GA_ReadOnly);
    auto ds2 = (GDALDataset *) GDALOpen(file_2, GA_ReadOnly);

    if(!ds1 || !ds2)
    {
        exit(1);
    }

    auto b1 = ds1->GetRasterBand(BAND_NR1);
    auto b2 = ds2->GetRasterBand(BAND_NR2);

    if(b1->GetXSize() != b2->GetXSize() || b1->GetYSize() != b2->GetYSize())
    {
        printf("dimensions mismatch, fn1 = (%d , %d), fn2 = (%d , %d)\n", b1->GetXSize(), b1->GetYSize(), b2->GetXSize(), b2->GetYSize());
        std::exit(1);
    }

    int w = b1->GetXSize();
    int h = b1->GetYSize();

    const char* OUT_FILE = "/tmp/clr_diff.tiff";

    auto ds_out = GetGDALDriverManager()->GetDriverByName("gtiff")->Create(OUT_FILE, w, h, 3, GDT_Byte, nullptr);

    auto ds_out2 = GetGDALDriverManager()->GetDriverByName("gtiff")->Create("/tmp/rel_diff.tiff", w, h, 1, GDT_Float32, nullptr);

    printf("(w, h) = %d , %d\n", w, h);

    std::vector<float> data1(w*h);
    std::vector<float> data2(w*h);

    std::vector<PixelData> diff_vec(w*h);



    b1->RasterIO(GF_Read, 0, 0, w, h, data1.data(), w, h, GDT_Float32, 0, 0);
    b2->RasterIO(GF_Read, 0, 0, w, h, data2.data(), w, h, GDT_Float32, 0, 0);
    GDALClose(ds1);
    GDALClose(ds2);


    std::vector<uint8_t> r_vec(w*h);
    std::vector<uint8_t> g_vec(w*h);
    std::vector<uint8_t> b_vec(w*h);

    std::vector<float> rel_diff_vec(w*h);

    size_t bad_pixels = 0;
    for(int i = 0; i < diff_vec.size(); i++)
    {

        auto& d = diff_vec[i];
        d.x = i % w;
        d.y = i / w;


        d.pixel_1 = data1[i];
        d.pixel_2 = data2[i];
        d.diff = data1[i] - data2[i];
        d.rel_diff = -1;
        if(data1[i] == 0 && data2[i] == 0)
        {
            d.rel_diff = 0;
        }
        else if(data2[i] == 0 || data1[i] == 0)
        {
            d.rel_diff = 0;
            bad_pixels++;

            d.print();

            r_vec[i] = 255;
        }
        else
        {
            d.rel_diff = d.diff/data2[i];
            rel_diff_vec[i] = d.rel_diff * 1e6;
            double diff = abs(d.rel_diff);
            if(diff != 0)
            {
                if(diff > 1e-1)
                {
                    r_vec[i] = 255;
                    g_vec[i] = 128;
                }
                else if(diff > 1e-2)
                {
                    r_vec[i] = 255;
                    g_vec[i] = 255;
                }
                else if(diff > 1e-3)
                {
                    g_vec[i] = 255;
                }
                else if(diff > 1e-4)
                {
                    g_vec[i] = 255;
                    b_vec[i] = 255;
                }
                else if(diff > 1e-5)
                {
                    g_vec[i] = 128;
                    b_vec[i] = 255;
                }
                else
                {
                    b_vec[i] = 255;
                }
            }
        }
    }



    printf("bad pixels = %zu\n", bad_pixels);
    printf("bad pixels = %f%%\n", (100.0 * bad_pixels)/(w*h));
    printf("bad pixels = %f ppm\n", (1e6 * bad_pixels)/(w*h));


    std::sort(diff_vec.begin(), diff_vec.end(), []
            (const PixelData& a, const PixelData& b)
    { return std::abs(a.rel_diff) > std::abs(b.rel_diff); });

    int n = 10;//0.001 * diff_vec.size();

    double avg_rel_diff = 0.0;
    size_t cnt = 0;

    for(auto e : diff_vec)
    {
        if(e.rel_diff != 0) {
            avg_rel_diff += std::abs(e.rel_diff);
            cnt++;
        }
    }

    if(cnt > 0) {
        for(int i = 0; i < n; i++)
        {
            diff_vec[i].print();
        }
        avg_rel_diff /= cnt;
        printf("avg rel diff = %f%%\n", avg_rel_diff * 100);
        printf("avg rel diff = %.15f ppm, cnt = %zu, pct = %f\n", avg_rel_diff * 1e6, cnt,
               ((double) cnt * 100) / (w * h));
        printf("median = %f ppm\n", diff_vec[cnt / 2].rel_diff * 1e6);
    }
    else
    {
        printf("cnt = %zu\n", cnt);
    }

    diff_vec.clear();
    diff_vec.shrink_to_fit();

    printf("writing file: %s\n", OUT_FILE);

    //ds_out->GetRasterBand(1)->SetNoDataValue(0);
    //ds_out->GetRasterBand(2)->SetNoDataValue(0);
    //ds_out->GetRasterBand(3)->SetNoDataValue(0);

    ds_out->GetRasterBand(1)->DeleteNoDataValue();
    ds_out->GetRasterBand(2)->DeleteNoDataValue();
    ds_out->GetRasterBand(3)->DeleteNoDataValue();



    ds_out->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, w, h, r_vec.data(), w, h, GDT_Byte, 0, 0);
    ds_out->GetRasterBand(2)->RasterIO(GF_Write, 0, 0, w, h, g_vec.data(), w, h, GDT_Byte, 0, 0);
    ds_out->GetRasterBand(3)->RasterIO(GF_Write, 0, 0, w, h, b_vec.data(), w, h, GDT_Byte, 0, 0);



    std::vector<std::vector<uint8_t>> tmps =
            {
                    {255, 0, 0},
                    {255, 127, 0},
                    {255, 255, 0},
                    {0, 255, 0},
                    {0, 255, 255},
                    {0, 127, 255},
                    {0, 0, 255}
            };

#if 0
    size_t off = 0;

    for(auto& e : tmps)
    {
        int w = 100;
        int h = 100;
        for(size_t i = 0; i < 3 ; i++)
        {
            std::vector<uint8_t> v(w*h, e.at(i));
            ds_out->GetRasterBand(i + 1)->RasterIO(GF_Write, 0, h * off, w, h, v.data(), w, h, GDT_Byte, 0, 0);
        }
        off++;
    }
#endif
    {
        int w = 1;
        int h = 255;

        std::vector<uint8_t> d(255);
        uint8_t c = 0;
        for(auto& e : d) e = c++;

        ds_out->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, w, h, d.data(), w, h, GDT_Byte, 0, 0);
        ds_out->GetRasterBand(2)->RasterIO(GF_Write, 0, 0, w, h, d.data(), w, h, GDT_Byte, 0, 0);
        ds_out->GetRasterBand(3)->RasterIO(GF_Write, 0, 0, w, h, d.data(), w, h, GDT_Byte, 0, 0);

    }


    ds_out2->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, w, h, rel_diff_vec.data(), w, h, GDT_Float32, 0, 0);

    print_gdal_cache();
    GDALClose(ds_out);
    GDALClose(ds_out2);
    print_gdal_cache();

}