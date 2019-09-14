#ifndef PARSIAN_CONTROL_H
#define PARSIAN_CONTROL_H





void Soccer::set_robot_wheel(std::size_t id, double leftWheel, double rightWheel) {
    if (id < 0 || id > info.number_of_robots - 1) {
        std::cerr << "trying to set an out of index robot wheel" << std::endl;
    }
    wheels[2 * id] = leftWheel;
    wheels[2 * id + 1] = rightWheel;
}

void Soccer::set_robot_vel(std::size_t id, double vel_f, double angle, double max_vel) {
    double error{rcsc::AngleDeg::normalize_angle(angle - worldModel.ourRobots[id].theta)};


    vel_f = fmin(vel_f, max_vel);

    if (fabs(error) > 90) {
        error = rcsc::AngleDeg::normalize_angle(angle + 180 - worldModel.ourRobots[id].theta);
        vel_f *= -1;
    }
    double vel_w = -PID_ang[id].run(error);

    // enhancement
    vel_f *= (90 - fabs(error)) / 50;

    vel_w = fmin(vel_w, .1);
    vel_w = fmax(vel_w, -.1);
//    std::cout<<"vel_f"<<vel_f<<std::endl;
//    std::cout<<"vel_w"<<vel_w<<std::endl;
    set_robot_wheel(id, -vel_f + vel_w, -vel_f - vel_w);
}


void Soccer::gotopoint(std::size_t id, rcsc::Vector2D pos, double max_vel, double theta) {
    auto dir = -rcsc::AngleDeg::normalize_angle((worldModel.ourRobots[id].pos - pos).dir().degree());
    auto error = worldModel.ourRobots[id].pos.dist(pos);
    auto thr = error * 6 + 10;
    thr = fmin(thr, 45);

    auto abs_error = fabs(rcsc::AngleDeg::normalize_angle(dir - worldModel.ourRobots[id].theta));
    double normal_vel = PID_pos[id].run(error);
    normal_vel = ((abs_error < thr || (180 - abs_error) < thr) ? normal_vel : 0);


    if (error < .03) {
        dir = theta;
        normal_vel = 0;
    }
    auto coef = 1.0;
    if (abs_error < 30)
        coef = 4.0 *(30 - abs_error) / 30.0;
    set_robot_vel(id, normal_vel * coef, dir, max_vel);
}


void Soccer::onetouch(std::size_t id, rcsc::Vector2D pos, double theta) {
    auto dir = rcsc::AngleDeg::normalize_angle((worldModel.ourRobots[id].pos - pos).dir().degree());
    auto error = worldModel.ourRobots[id].pos.dist(pos);
    auto thr = error * 12;
    double max_vel = 1;
    thr = fmin(thr, 15);
    thr = fmax(thr, 3);

    auto abs_error = fabs(rcsc::AngleDeg::normalize_angle(dir - worldModel.ourRobots[id].theta));
    double normal_vel = PID_pos[id].run(error);
    normal_vel = ((abs_error < thr || (180 - abs_error) < thr) ? normal_vel : 0);

    if (error < .5 || (abs_error < 3 || (180 - abs_error) < 3)) {
        normal_vel = 10.0;
        max_vel = 20.0;
    }

    set_robot_vel(id, normal_vel, dir, max_vel);
}

void Soccer::kick(int id, const rcsc::Vector2D &targetPos) {

    double ROBOT_WIDTH{info.robot_size[id]};
    double ROBOT_HALF_WIDTH{ROBOT_WIDTH * 1};
    if (state != STATE::AVOID)
        ROBOT_HALF_WIDTH = {ROBOT_WIDTH * .5};
    double Field_width{info.field[0]};
    double Field_height{info.field[1]};


    rcsc::Vector2D ballPos{worldModel.ball.pos + worldModel.ball.vel * (.04)};
    rcsc::Vector2D robotPos{worldModel.ourRobots[id].pos + worldModel.ourRobots[id].vel * (.005)};
    rcsc::Vector2D norm{ballPos - targetPos};
    norm = norm.normalize();
    rcsc::Vector2D prependicular{norm.rotatedVector(90)};
    rcsc::Vector2D behindPos{ballPos + norm * .4 + worldModel.ball.vel * .12};
    rcsc::Vector2D avoidPos{ballPos + prependicular * .4};
    rcsc::Circle2D robotArea{robotPos, ROBOT_HALF_WIDTH * sqrt(2)};
    rcsc::Vector2D sol1, sol2;
    if (behindPosIsValid(targetPos)) {
        behindPos = lastBehinePos;
    } else {
        lastBehinePos = behindPos;
    }
    if (robotArea.intersection(rcsc::Segment2D{ballPos, targetPos}, &sol1, &sol2) > 0) {
        if (state == STATE::KICK) {
            if (robotPos.x > worldModel.ball.pos.x)
                state = STATE::AVOID;
        } else
            state = STATE::AVOID;

    } else if (state != STATE::KICK) {
        if (worldModel.ourRobots[id].pos.dist(behindPos) < .1)
            state = STATE::KICK;
        else
            state = STATE::BEHIND;
    } else
    if (robotPos.x > worldModel.ball.pos.x + .1)
        state = state = STATE::BEHIND;


//    if (avoidPos.x < -Field_width / 2 + ROBOT_WIDTH || avoidPos.x > Field_width / 2 + ROBOT_WIDTH ||
//        avoidPos.y < -Field_height / 2 + ROBOT_WIDTH || avoidPos.x > Field_height / 2 + ROBOT_WIDTH)
//        state = STATE::KICK;

    switch (state) {

        case AVOID:
            gotopoint(id, avoidPos);
            std::cout << "avoid" << std::endl;

            break;
        case BEHIND:
            gotopoint(id, behindPos);
            std::cout << "behind" << std::endl;

            break;
        case KICK:
            if (ballPos.dist(robotPos) < .25) {
                //fast kick

                gotopoint(id, ballPos - norm * 2, 3);

            } else
                gotopoint(id, ballPos, 5);
            std::cout << "kikckkkkkk" << std::endl;

            break;
    }

}



void Soccer::validatePos(rcsc::Vector2D &targetPos) {
    if(targetPos.x > info.field[0]/2)   targetPos.x = info.field[0]/2 - 0.08;
    if(targetPos.x < -info.field[0]/2)   targetPos.x = -info.field[0]/2 + 0.08;
    if(targetPos.y > info.field[1]/2)   targetPos.y = info.field[1]/2 - 0.08;
    if(targetPos.y < -info.field[1]/2)   targetPos.y = -info.field[1]/2 + 0.08;
}

bool Soccer::behindPosIsValid(const rcsc::Vector2D &targetPos) {
    rcsc::Circle2D robotArea{lastBehinePos, .5};
    rcsc::Vector2D ballPos{worldModel.ball.pos};
    rcsc::Ray2D path{ballPos, targetPos};
    rcsc::Vector2D sol1, sol2;

    return !(robotArea.intersection(path, &sol1, &sol2) > 0 || lastBehinePos.dist(ballPos) > 1);
}

#endif //PARSIAN_CONTROL_H
