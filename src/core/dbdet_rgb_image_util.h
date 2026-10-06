// Convert loaded images (RGB, RGBA, or grey) to vil_rgb<vxl_byte>.
#ifndef dbdet_rgb_image_util_h_
#define dbdet_rgb_image_util_h_

#include <vil/vil_image_resource.h>
#include <vil/vil_image_view.h>
#include <vil/vil_rgb.h>
#include <vil/vil_rgba.h>
#include <vcl_iostream.h>

inline bool dbdet_get_rgb_view(const vil_image_resource_sptr& image_sptr,
                               vil_image_view<vil_rgb<vxl_byte> >& rgb)
{
  if (!image_sptr)
    return false;

  vil_image_view_base_sptr view = image_sptr->get_view();
  if (!view)
    return false;

  // Already RGB
  if (vil_image_view<vil_rgb<vxl_byte> >* p =
        dynamic_cast<vil_image_view<vil_rgb<vxl_byte> >*>(view.as_pointer())) {
    rgb = *p;
    return rgb.ni() > 0 && rgb.nj() > 0;
  }

  // RGBA -> drop alpha
  if (vil_image_view<vil_rgba<vxl_byte> >* p =
        dynamic_cast<vil_image_view<vil_rgba<vxl_byte> >*>(view.as_pointer())) {
    rgb.set_size(p->ni(), p->nj());
    for (unsigned j = 0; j < p->nj(); ++j)
      for (unsigned i = 0; i < p->ni(); ++i) {
        const vil_rgba<vxl_byte>& px = (*p)(i, j);
        rgb(i, j) = vil_rgb<vxl_byte>(px.R(), px.G(), px.B());
      }
    return true;
  }

  // Multi-plane byte image (e.g. PNG loaded as nplanes==3 or 4)
  if (vil_image_view<vxl_byte>* p =
        dynamic_cast<vil_image_view<vxl_byte>*>(view.as_pointer())) {
    rgb.set_size(p->ni(), p->nj());
    if (p->nplanes() >= 3) {
      for (unsigned j = 0; j < p->nj(); ++j)
        for (unsigned i = 0; i < p->ni(); ++i)
          rgb(i, j) = vil_rgb<vxl_byte>((*p)(i, j, 0), (*p)(i, j, 1), (*p)(i, j, 2));
      return true;
    }
    if (p->nplanes() == 1) {
      for (unsigned j = 0; j < p->nj(); ++j)
        for (unsigned i = 0; i < p->ni(); ++i) {
          vxl_byte g = (*p)(i, j);
          rgb(i, j) = vil_rgb<vxl_byte>(g, g, g);
        }
      return true;
    }
  }

  vcl_cerr << "dbdet_get_rgb_view: unsupported pixel format\n";
  return false;
}

#endif // dbdet_rgb_image_util_h_
