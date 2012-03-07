struct Mouse : Controller {
  uint2 data();
  void latch(bool data);
  Mouse(bool port);
  void serialize(serializer& s);
private:
  bool latched;
  unsigned counter;
  int _position_x;
  int _position_y;
};
