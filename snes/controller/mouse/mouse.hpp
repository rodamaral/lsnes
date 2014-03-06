struct Mouse : Controller {
  uint2 data();
  void latch(bool data);
  Mouse(bool port);
  void serialize(serializer& s);
private:
  bool latched;
  unsigned counter;
  unsigned speed;
  int _position_x;
  int _position_y;
};
