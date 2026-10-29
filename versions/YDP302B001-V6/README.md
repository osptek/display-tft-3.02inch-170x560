<p align="left"><img alt="OSPTEK" src="../../images/logo.png" width="200" /></p>

<h1 align="center">OSPTEK 3.02″ TFT 显示屏 170×560（YDP302B001-V6）</h1>

<p align="center"><b>TFT 模组 · SPI · AXS15231B</b></p>

<p align="center"><a href="./README_EN.md">English</a> | 简体中文 · <a href="../../README.md">规格族索引</a></p>

<p align="center">
  <img alt="Interface: SPI" src="https://img.shields.io/badge/Interface-SPI-27AE60?style=flat-square" />
  <img alt="Driver: AXS15231B" src="https://img.shields.io/badge/Driver-AXS15231B-E7352C?style=flat-square" />
</p>

<p align="center"><img alt="OSPTEK 3.02 寸 170×560 TFT 模组宣传图" src="./images/product.png" width="640" /></p>

## 目录

- [规格参数](#规格参数)
- [相关资料](#相关资料)
- [仓库结构](#仓库结构)
- [购买链接](#购买链接)
- [技术支持](#技术支持)

---

## 规格参数

| 项目 | 规格 |
| ---- | ---- |
| 尺寸 | 3.02 英寸 |
| 类型 | TFT |
| 分辨率 | 170×560 |
| 外形尺寸 | 24.63 x 78.41 mm |
| 有效显示区域 | 22.134 x 72.912 mm |
| 接口 | SPI |
| 驱动 IC | AXS15231B |
| 触摸驱动 | — |

> 完整外形尺寸、FPC 定义、供电与时序以产品规格书 / 驱动手册为准。

## 相关资料

| 分类 | 文件 |
| ---- | ---- |
| 规格文档 | [`AXS15231B_Datasheet_V0.9_20240221-客户版.pdf`](./datasheets/AXS15231B_Datasheet_V0.9_20240221-%E5%AE%A2%E6%88%B7%E7%89%88.pdf) |
| 规格文档 | [`3.02寸屏幕测试板V1.1.pdf`](./datasheets/3.02%E5%AF%B8%E5%B1%8F%E5%B9%95%E6%B5%8B%E8%AF%95%E6%9D%BFV1.1.pdf) |
| 规格文档 | [`YDP302B001-V6.pdf`](./datasheets/YDP302B001-V6.pdf) |
| CAD 图纸 | [`YDP302B001-V6+盖板.dwg`](./cad/YDP302B001-V6%2B%E7%9B%96%E6%9D%BF.dwg) |
| 转接板资料 | [`3.02寸转接板.pdf`](./adapter/3.02%E5%AF%B8%E8%BD%AC%E6%8E%A5%E6%9D%BF.pdf) |

## 仓库结构

```text
display-tft-3.02inch-170x560/ # 仓库根（导航见 ../../README.md，示例工程见根目录 examples/）
└── versions/
    └── YDP302B001-V6/        # 本料号完整资料
        ├── README.md
        ├── README_EN.md
        ├── osptek.yml
        ├── images/
        ├── datasheets/       # 产品规格书 + 驱动/触摸 IC 数据手册
        ├── cad/              # 外形/盖板 CAD
        └── adapter/          # 转接板资料 + 转接板图片
```

## 购买链接

见[仓库首页](../../README.md#购买链接)。

## 技术支持

见[仓库首页](../../README.md#技术支持)。

---

<p align="center"><sub>© 2026 OSPTEK 鱼鹰光电 · 本仓库资料采用 CC BY 4.0 许可</sub></p>
