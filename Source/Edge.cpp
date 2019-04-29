using glm::vec4;

class Edge {
  public:
    vec4 p1;
    vec4 p2;

    Edge(vec4 p1, vec4 p2) {
      this->p1 = p1;
      this->p2 = p2;
    }

    bool operator ==(const Edge& e) const {
      if (this->p1.x == e.p1.x && this->p1.y == e.p1.y && this->p1.z == e.p1.z &&
          this->p2.x == e.p2.x && this->p2.y == e.p2.y && this->p2.z == e.p2.z) {
        return true;
      } else {
        return false;
      }
    }
};
