/* GStreamer unit tests for playbin
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/base/gstpushsrc.h>
#include <unistd.h>

#ifndef GST_DISABLE_REGISTRY

static GType gst_red_video_src_get_type (void);
static GType gst_codec_src_get_type (void);

#define DEFINE_TEST(func) \
    static void func (void);                            \
    \
    GST_START_TEST(func ## _decodebin1)                  \
    { g_unsetenv("USE_DECODEBIN2"); func(); }            \
    GST_END_TEST;                                        \
    \
    GST_START_TEST(func ## _decodebin2)                  \
    { g_setenv("USE_DECODEBIN2", "1", TRUE); func(); }   \
    GST_END_TEST;

DEFINE_TEST (test_sink_usage_video_only_stream);

/* make sure the audio sink is not touched for video-only streams */
static void
test_sink_usage_video_only_stream (void)
{
  GstElement *playbin, *fakevideosink, *fakeaudiosink;
  GstState cur_state, pending_state;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakevideosink = gst_element_factory_make ("fakesink", "fakevideosink");
  fail_unless (fakevideosink != NULL, "Failed to create fakevideosink element");

  fakeaudiosink = gst_element_factory_make ("fakesink", "fakeaudiosink");
  fail_unless (fakeaudiosink != NULL, "Failed to create fakeaudiosink element");

  /* video-only stream, audiosink will error out in null => ready if used */
  g_object_set (fakeaudiosink, "state-error", 1, NULL);

  g_object_set (playbin, "video-sink", fakevideosink, NULL);
  g_object_set (playbin, "audio-sink", fakeaudiosink, NULL);

  g_object_set (playbin, "uri", "redvideo://", NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  fail_unless_equals_int (gst_element_get_state (fakeaudiosink, &cur_state,
          &pending_state, 0), GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (cur_state, GST_STATE_NULL);
  fail_unless_equals_int (pending_state, GST_STATE_VOID_PENDING);

  {
    GValueArray *stream_info = NULL;

    g_object_get (playbin, "stream-info-value-array", &stream_info, NULL);
    fail_unless (stream_info != NULL);
    fail_unless_equals_int (stream_info->n_values, 1);
    g_value_array_free (stream_info);
  }

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

/* this tests async error handling when setting up the subbin */
DEFINE_TEST (test_suburi_error_unknowntype);

static void
test_suburi_error_unknowntype (void)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* suburi file format unknown: playbin should just ignore the suburi and
   * preroll normally (if /dev/zero does not exist, this test should behave
   * the same as test_suburi_error_invalidfile() */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "file:///dev/zero", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

DEFINE_TEST (test_suburi_error_invalidfile);

static void
test_suburi_error_invalidfile (void)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* suburi file does not exist: playbin should just ignore the suburi and
   * preroll normally */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "file:///foo/bar/803129999/32x9ax1", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

DEFINE_TEST (test_suburi_error_wrongproto);

static void
test_suburi_error_wrongproto (void)
{
  GstElement *playbin, *fakesink;

  fail_unless (gst_element_register (NULL, "redvideosrc", GST_RANK_PRIMARY,
          gst_red_video_src_get_type ()));

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink = gst_element_factory_make ("fakesink", "fakesink");
  fail_unless (fakesink != NULL, "Failed to create fakesink element");
  ASSERT_OBJECT_REFCOUNT (fakesink, "fakesink after creation", 1);

  g_object_set (playbin, "video-sink", fakesink, NULL);

  /* wrong protocol for suburi: playbin should just ignore the suburi and
   * preroll normally */
  g_object_set (playbin, "uri", "redvideo://", NULL);
  g_object_set (playbin, "suburi", "nosuchproto://foo.bar:80", NULL);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (playbin, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

static GstElement *
create_playbin (const gchar * uri)
{
  GstElement *playbin, *fakesink1, *fakesink2;

  playbin = gst_element_factory_make ("playbin", "playbin");
  fail_unless (playbin != NULL, "Failed to create playbin element");

  fakesink1 = gst_element_factory_make ("fakesink", NULL);
  fail_unless (fakesink1 != NULL, "Failed to create fakesink element #1");

  fakesink2 = gst_element_factory_make ("fakesink", NULL);
  fail_unless (fakesink2 != NULL, "Failed to create fakesink element #2");

  /* make them behave like normal sinks, even if not needed for the test */
  g_object_set (fakesink1, "sync", TRUE, NULL);
  g_object_set (fakesink2, "sync", TRUE, NULL);

  g_object_set (playbin, "video-sink", fakesink1, NULL);
  g_object_set (playbin, "audio-sink", fakesink2, NULL);

  g_object_set (playbin, "uri", uri, NULL);

  return playbin;
}

DEFINE_TEST (test_missing_urisource_handler);

static void
test_missing_urisource_handler (void)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;

  playbin = create_playbin ("chocchipcookie://withahint.of/cinnamon");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  /* there should be at least a missing-plugin message on the bus now and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (msg->structure != NULL);
  s = msg->structure;
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"),
      "chocchipcookie");
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a CORE MISSING_PLUGIN one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
      "%s instead of core-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
      "code %u instead of GST_CORE_ERROR_MISSING_PLUGIN", err->code);
  g_error_free (err);
  gst_message_unref (msg);
  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

DEFINE_TEST (test_missing_suburisource_handler);

static void
test_missing_suburisource_handler (void)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;

  playbin = create_playbin ("file:///does/not/exis.t");

  g_object_set (playbin, "suburi", "cookie://withahint.of/cinnamon", NULL);

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_FAILURE);

  /* there should be at least a missing-plugin message on the bus now and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (msg->structure != NULL);
  s = msg->structure;
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "cookie");
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a CORE MISSING_PLUGIN one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
      "%s instead of core-error-quark", g_quark_to_string (err->domain));
  fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
      "code %u instead of GST_CORE_ERROR_MISSING_PLUGIN", err->code);
  g_error_free (err);
  gst_message_unref (msg);
  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

DEFINE_TEST (test_missing_primary_decoder);

static void
test_missing_primary_decoder (void)
{
  GstStructure *s;
  GstMessage *msg;
  GstElement *playbin;
  GError *err = NULL;
  GstBus *bus;
  gchar *use_decodebin2 = getenv ("USE_DECODEBIN2");
  gboolean decodebin2 = use_decodebin2 != NULL && *use_decodebin2 == '1';

  fail_unless (gst_element_register (NULL, "codecsrc", GST_RANK_PRIMARY,
          gst_codec_src_get_type ()));

  playbin = create_playbin ("codec://");

  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  fail_unless_equals_int (gst_element_set_state (playbin, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* there should soon be at least a missing-plugin message on the bus and an
   * error message; the missing-plugin message should be first */
  bus = gst_element_get_bus (playbin);

  msg = gst_bus_poll (bus, GST_MESSAGE_ELEMENT | GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (msg->structure != NULL);
  s = msg->structure;
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "decoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  gst_message_unref (msg);

  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, -1);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ERROR);

  /* make sure the error is a STREAM CODEC_NOT_FOUND one */
  gst_message_parse_error (msg, &err, NULL);
  fail_unless (err != NULL);
  if (decodebin2) {
    fail_unless (err->domain == GST_CORE_ERROR, "error has wrong error domain "
        "%s instead of core-error-quark", g_quark_to_string (err->domain));
    fail_unless (err->code == GST_CORE_ERROR_MISSING_PLUGIN, "error has wrong "
        "code %u instead of GST_RESOURCE_ERROR_MISSING_PLUGIN", err->code);
  } else {
    fail_unless (err->domain == GST_STREAM_ERROR,
        "error has wrong error domain " "%s instead of stream-error-quark",
        g_quark_to_string (err->domain));
    fail_unless (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND,
        "error has wrong "
        "code %u instead of GST_STREAM_ERROR_CODEC_NOT_FOUND", err->code);
  }
  g_error_free (err);
  gst_message_unref (msg);
  gst_object_unref (bus);

  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_object_unref (playbin);
}

/*** redvideo:// source ***/

static GstURIType
gst_red_video_src_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_red_video_src_uri_get_protocols (void)
{
  static gchar *protocols[] = { (char *) "redvideo", NULL };

  return protocols;
}

static const gchar *
gst_red_video_src_uri_get_uri (GstURIHandler * handler)
{
  return "redvideo://";
}

static gboolean
gst_red_video_src_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  return (uri != NULL && g_str_has_prefix (uri, "redvideo:"));
}

static void
gst_red_video_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_red_video_src_uri_get_type;
  iface->get_protocols = gst_red_video_src_uri_get_protocols;
  iface->get_uri = gst_red_video_src_uri_get_uri;
  iface->set_uri = gst_red_video_src_uri_set_uri;
}

static void
gst_red_video_src_init_type (GType type)
{
  static const GInterfaceInfo uri_hdlr_info = {
    gst_red_video_src_uri_handler_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_hdlr_info);
}

typedef GstPushSrc GstRedVideoSrc;
typedef GstPushSrcClass GstRedVideoSrcClass;

GST_BOILERPLATE_FULL (GstRedVideoSrc, gst_red_video_src, GstPushSrc,
    GST_TYPE_PUSH_SRC, gst_red_video_src_init_type);

static void
gst_red_video_src_base_init (gpointer klass)
{
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("video/x-raw-yuv, format=(fourcc)I420")
      );
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_templ));
  gst_element_class_set_details_simple (element_class,
      "Red Video Src", "Source/Video", "yep", "me");
}

static GstFlowReturn
gst_red_video_src_create (GstPushSrc * src, GstBuffer ** p_buf)
{
  GstBuffer *buf;
  GstCaps *caps;
  guint8 *data;
  guint w = 64, h = 64;
  gsize size;

  size = w * h * 3 / 2;
  buf = gst_buffer_new_and_alloc (size);
  data = gst_buffer_map (buf, NULL, NULL, GST_MAP_WRITE);
  memset (data, 76, w * h);
  memset (data + (w * h), 85, (w * h) / 4);
  memset (data + (w * h) + ((w * h) / 4), 255, (w * h) / 4);
  gst_buffer_unmap (buf, data, size);

  caps = gst_caps_new_simple ("video/x-raw-yuv", "format", GST_TYPE_FOURCC,
      GST_MAKE_FOURCC ('I', '4', '2', '0'), "width", G_TYPE_INT, w, "height",
      G_TYPE_INT, h, "framerate", GST_TYPE_FRACTION, 1, 1, NULL);
  gst_buffer_set_caps (buf, caps);
  gst_caps_unref (caps);

  *p_buf = buf;
  return GST_FLOW_OK;
}

static void
gst_red_video_src_class_init (GstRedVideoSrcClass * klass)
{
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  pushsrc_class->create = gst_red_video_src_create;
}

static void
gst_red_video_src_init (GstRedVideoSrc * src, GstRedVideoSrcClass * klass)
{
}

/*** codec:// source ***/

static GstURIType
gst_codec_src_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_codec_src_uri_get_protocols (void)
{
  static gchar *protocols[] = { (char *) "codec", NULL };

  return protocols;
}

static const gchar *
gst_codec_src_uri_get_uri (GstURIHandler * handler)
{
  return "codec://";
}

static gboolean
gst_codec_src_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  return (uri != NULL && g_str_has_prefix (uri, "codec:"));
}

static void
gst_codec_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_codec_src_uri_get_type;
  iface->get_protocols = gst_codec_src_uri_get_protocols;
  iface->get_uri = gst_codec_src_uri_get_uri;
  iface->set_uri = gst_codec_src_uri_set_uri;
}

static void
gst_codec_src_init_type (GType type)
{
  static const GInterfaceInfo uri_hdlr_info = {
    gst_codec_src_uri_handler_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_hdlr_info);
}

#undef parent_class
#define parent_class codec_src_parent_class

typedef GstPushSrc GstCodecSrc;
typedef GstPushSrcClass GstCodecSrcClass;

GST_BOILERPLATE_FULL (GstCodecSrc, gst_codec_src, GstPushSrc,
    GST_TYPE_PUSH_SRC, gst_codec_src_init_type);

static void
gst_codec_src_base_init (gpointer klass)
{
  static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("application/x-codec")
      );
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_templ));
  gst_element_class_set_details_simple (element_class,
      "Codec Src", "Source/Video", "yep", "me");
}

static GstFlowReturn
gst_codec_src_create (GstPushSrc * src, GstBuffer ** p_buf)
{
  GstBuffer *buf;
  GstCaps *caps;
  guint8 *data;

  buf = gst_buffer_new_and_alloc (20);
  data = gst_buffer_map (buf, NULL, NULL, GST_MAP_WRITE);
  memset (data, 0, 20);
  gst_buffer_unmap (buf, data, 20);

  caps = gst_caps_new_simple ("application/x-codec", NULL);
  gst_buffer_set_caps (buf, caps);
  gst_caps_unref (caps);

  *p_buf = buf;
  return GST_FLOW_OK;
}

static void
gst_codec_src_class_init (GstCodecSrcClass * klass)
{
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  pushsrc_class->create = gst_codec_src_create;
}

static void
gst_codec_src_init (GstCodecSrc * src, GstCodecSrcClass * klass)
{
}

#endif /* GST_DISABLE_REGISTRY */


static Suite *
playbin_suite (void)
{
  Suite *s = suite_create ("playbin");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

#ifndef GST_DISABLE_REGISTRY
  /* with the old decodebin */
  tcase_add_test (tc_chain, test_sink_usage_video_only_stream_decodebin1);
  tcase_add_test (tc_chain, test_suburi_error_wrongproto_decodebin1);
  tcase_add_test (tc_chain, test_suburi_error_invalidfile_decodebin1);
  tcase_add_test (tc_chain, test_suburi_error_unknowntype_decodebin1);
  tcase_add_test (tc_chain, test_missing_urisource_handler_decodebin1);
  tcase_add_test (tc_chain, test_missing_suburisource_handler_decodebin1);
  tcase_add_test (tc_chain, test_missing_primary_decoder_decodebin1);

  /* and again with decodebin2 */
  tcase_add_test (tc_chain, test_missing_primary_decoder_decodebin2);
  tcase_add_test (tc_chain, test_sink_usage_video_only_stream_decodebin2);
  tcase_add_test (tc_chain, test_suburi_error_wrongproto_decodebin2);
  tcase_add_test (tc_chain, test_suburi_error_invalidfile_decodebin2);
  tcase_add_test (tc_chain, test_suburi_error_unknowntype_decodebin2);
  tcase_add_test (tc_chain, test_missing_urisource_handler_decodebin2);
  tcase_add_test (tc_chain, test_missing_suburisource_handler_decodebin2);

  /* one day we might also want to have the following checks:
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_one_fatal);
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_two_fatal);
   * tcase_add_test (tc_chain, test_missing_secondary_decoder_two_with_preroll);
   */
#endif

  return s;
}

GST_CHECK_MAIN (playbin);
